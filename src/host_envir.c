/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

host_envir.c -- Host-environment-dependent routines.

(These have to do with operating system and file name differences.)

This version for UNIX, MS-DOS, VAX/VMS, and Windows NT.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#if EDG_WIN32
/* Disable unneeded features of <windows.h> for efficiency. */
#define NOGDICAPMASKS
#define NOVIRTUALKEYCODES
#define NOWINMESSAGES
#define NOWINSTYLES
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOSHOWWINDOW
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOGDI
#define NOKERNEL
#define NOMB
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NOCRYPT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#if !defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
/*
If ENABLE_VIRTUAL_TERMINAL_PROCESSING was not defined (as is the case with
some older Microsoft SDKs), define it here.
*/
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif /* !defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING) */
#if CPPCLI_ENABLING_POSSIBLE
#include <metahost.h>
#endif /* CPPCLI_ENABLING_POSSIBLE */
#endif /* EDG_WIN32 */

#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ms_metadata.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if __MICROSOFT_OS__
/* Include file layout for MS-DOS compilers. */
#if __TURBOC__
/* Need dir.h for lengths of path components. */
#include <dir.h>
#else /* __TURBOC__ */
#if __MSC__
/* Need process.h for definition of getpid(). */
#include <process.h>
#endif /* __MSC__ */
#endif /* __TURBOC__ */
#endif /* __MICROSOFT_OS__ */

/*
Define __AIX__ if _AIX is defined.
*/
#ifdef _AIX
#ifndef __AIX__
#define __AIX__ 1
#endif /* ifndef __AIX__ */
#endif /* ifdef _AIX */

/* ANSI signal handlers return void. Older UNIX signal handlers in general,
and SVID compliant signal handlers in particular, return int. */
#ifdef __ANSIC__
#define SIGNAL_HANDLER_RETURNS_VOID 1
#else /* !defined(__ANSIC__) */
#ifdef __sun
/* SunOS 4.1 switched to the ANSI form of signal. */
#define SIGNAL_HANDLER_RETURNS_VOID 1
#endif  /* __sun */
#ifdef __hpux
#define SIGNAL_HANDLER_RETURNS_VOID 1
#endif  /* __hpux */
#endif  /* __ANSIC__ */

#include <signal.h>
#ifdef SIGNAL_HANDLER_RETURNS_VOID
typedef void a_signal_handler_return_value;
#else /* !defined(SIGNAL_HANDLER_RETURNS_VOID) */
typedef int a_signal_handler_return_value;
#endif /* defined(SIGNAL_HANDLER_RETURNS_VOID) */

#include <errno.h>
#if __BSD__
/* BSD errno.h doesn't define "errno". */
extern "C" int errno;
#endif /* __BSD__ */

/*
Included to define ctime, etc.
*/
#include <time.h>

/*
Header files needed to use the system routines to get the elapsed clock
time and CPU time.  The ANSI routines are used when possible; otherwise
the UNIX routines are assumed to be available.

Note: If you are not using an ANSI C library make sure that
CLOCK_FREQUENCY is defined properly below.
*/
#if __ANSIC__
#ifndef CLOCKS_PER_SEC
 #error -- Compiling in __ANSIC__ mode but CLOCKS_PER_SEC is not
	    defined in time.h.
#endif /* defined(CLOCKS_PER_SEC) */
#else /* !__ANSIC__ */
#include <sys/types.h>
#include <sys/times.h>
#ifdef __sun
#include <sys/param.h>
#define CLOCK_FREQUENCY HZ
#else /* ifndef __sun */
#define CLOCK_FREQUENCY 60
#endif /* ifdef __sun */
#endif /* __ANSIC__ */

#if !__MICROSOFT_OS__
/* If we are not on MS-DOS, we assume that we are on some sort of
   Unix system.  Include unistd.h to get declarations for
   the system calls and library routines. */
#include <unistd.h>
#endif /* !__MICROSOFT_OS__ */

/*
Determine whether getcwd or getwd should be used to get the current
directory.  getwd is used on BSD, getcwd on other systems.
*/
#if __MICROSOFT_OS__
#define USE_GETCWD 1
#include <direct.h>
#else /* !__MICROSOFT_OS___ */
#if __BSD__
#include <sys/param.h>
#define USE_GETCWD 0
#else /* !__BSD__ */
#include <unistd.h>
#define USE_GETCWD 1
#endif /* __BSD__ */
#endif /* __MICROSOFT_OS__ */

#if __MICROSOFT_OS__
/* Function definitions for MS-DOS compilers. */
#if __TURBOC__ || __ZTC__
/* Neither Turbo C nor Zortech have the getpid call.  Since MSDOS does
   not have multiple tasks, just return a 1. */
static int getpid(void)
{
  return (1);
}
#endif /* __TURBOC__ */
#else /* __MICROSOFT_OS__ */
/* Function definitions for non MS-DOS compilers. */
#include <time.h>
#endif /* __MICROSOFT_OS__ */

/*
Define some maximum lengths for MS-DOS file name handling stuff.  Note that the
defined lengths INCLUDE the terminating null character.  "__MAXDRIVE__" is the
length of the disk drive letter which includes the trailing ":".  "__MAXDIR__"
is the length of the directory part of the path including a trailing slash.
"__MAXFILE__" is the length of the base file name.  "__MAXEXT__" is the length
of the extension including the leading ".".
*/
#if __MICROSOFT_OS__
#if __TURBOC__
#define __MAXDRIVE__ MAXDRIVE
#define __MAXDIR__   MAXDIR
#define __MAXFILE__  MAXFILE
#define __MAXEXT__   MAXEXT

#else /* __TURBOC__ */
#if __MSC__
#define __MAXDRIVE__ _MAX_DRIVE
#define __MAXDIR__   _MAX_DIR
#define __MAXFILE__  _MAX_FNAME
#define __MAXEXT__   _MAX_EXT
#else /* __MSC__ */
#if __ZTC__
/* Zortech doesn't currently (version 3.0) define these values. */
#define __MAXDRIVE__ 3
#define __MAXDIR__   66
#define __MAXFILE__  9
#define __MAXEXT__   5
#else /* __ZTC */
error -- unknown MS-DOS compiler.
#endif /* __ZTC */
#endif /* __MSC__ */
#endif /* __TURBOC__ */
#endif /* __MICROSOFT_OS__ */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED

STATIC_THREAD a_boolean
		locale_already_set;
			/* TRUE if the multibyte locale has been set. */

#if __MICROSOFT_OS__

static char *mbc_memchr(char		*str,
			int		chr,
			sizeof_t	size)
/*
This is a version of the memchr routine that also works properly for strings
containing multibyte characters.  Return the first occurrence of chr
in the first size bytes of str, or NULL if chr is not found.
*/
{
  char	*result = NULL;
  char	*p;
  char	*end = str + size - 1;

  for (p = str; p <= end; increment_mbc_ptr(p)) {
    if (*p == chr) {
      result = p;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* mbc_memchr */

#endif /* __MICROSOFT_OS__ */

a_const_char *mbc_strchr(a_const_char *str,
                         int          chr)
/*
This is a version of the strchr routine that also works properly for strings
containing multibyte characters.  Return the first occurrence of chr
in str, or NULL if chr does not occur in str.
*/
{
  a_const_char *result = NULL;
  a_const_char *p;

  for (p = str; *p != '\0'; increment_mbc_ptr(p)) {
    if (*p == chr) {
      result = p;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* mbc_strchr */


static a_const_char *mbc_strrchr(a_const_char *str,
                                 int          chr)
/*
This is a version of the strrchr routine that also works properly for strings
containing multibyte characters.  Return the last occurrence of chr
in str, or NULL if chr does not occur in str.
*/
{
  a_const_char *result = NULL;
  a_const_char *p;

  for (p = str; *p != '\0'; increment_mbc_ptr(p)) {
    if (*p == chr) result = p;
  }  /* for */
  return result;
}  /* mbc_strrchr */


#if __MICROSOFT_OS__

static sizeof_t truncate_length_to_whole_characters(a_const_char *str,
                                                    sizeof_t     length)
/*
Return the number of bytes of "str" that represent whole characters whose
total length does not exceed "length".  This is used to ensure that when
a string is truncated, it is done on a multibyte character boundary.
*/
{
  sizeof_t	result = length;
  a_const_char	*ptr;
  sizeof_t	last_len;
  sizeof_t	curr_len;

  for (ptr = str; *ptr != '\0';
       last_len = curr_len, increment_mbc_ptr(ptr)) {
    curr_len = ptr - str + mbc_length_simple(ptr);
    if (curr_len == length) {
      result = length;
      break;
    } else if (curr_len > length) {
      result = last_len;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* truncate_length_to_whole_characters */

#endif /* __MICROSOFT_OS__ */

#else /* !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

/*
When not using multibyte characters, just map these names onto the
normal C library routines.
*/
#define mbc_strrchr(str, chr) strrchr((str), (chr))

#if __MICROSOFT_OS__
#define mbc_memchr(str, chr, size) memchr((str), (chr), (size))
#endif /* __MICROSOFT_OS__ */

#if __MICROSOFT_OS__
/*
When not using multibyte characters, this routine just returns the
original length.
*/
#define truncate_length_to_whole_characters(str, length) (length)
#endif /* __MICROSOFT_OS__ */

#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

STATIC_THREAD a_text_buffer_ptr
		file_read_buffer;
			/* Buffer used when reading from the various files. */

STATIC_THREAD a_directory_name_entry_ptr
		avail_directory_name_entries;
			/* Available list of directory name entries. */

STATIC_THREAD a_directory_name_entry_ptr
		template_search_path_tail;
			/* End of the search path to find exported templates.
			   The name strings are in general storage. */

STATIC_THREAD a_text_buffer_ptr
		dir_and_file_buffer;
			/* A text buffer used by combine_dir_and_file_name.*/

STATIC_THREAD a_text_buffer_ptr
		format_file_name_buffer;
			/* A text buffer used by f_format_file_name.*/

#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE

STATIC_THREAD a_text_buffer_ptr
		utf8_buffer;
			/* A text buffer used by multibyte_chars_to_utf8 to
			   hold the UTF-8 result. */

STATIC_THREAD a_text_buffer_ptr
		mbc_buffer;
			/* A text buffer used by utf8_to_multibyte_char
			   to hold the native character result. */

#if EDG_WIN32
STATIC_THREAD a_text_buffer_ptr
		locale_name_buffer;
			/* A text buffer used by
			   get_system_default_locale_name. */

STATIC_THREAD _locale_t
		system_default_locale;
			/* The locale object for the Windows system default
			   locale.  This is the default locale used for
			   converting multibyte characters to UTF-8. */

#endif /* EDG_WIN32 */
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */

#if EDG_WIN32
STATIC_THREAD a_text_buffer_ptr
		conv_utf8_buffer;
			/* A text buffer used by conv_wide_to_utf8. */
#endif /* EDG_WIN32 */


STATIC_THREAD a_directory_name_entry_ptr
		dir_name_list_general;
			/* List of all directory name strings used that have
			   been allocated in general memory.  Used so that the
			   strings can be shared. */

static void free_directory_name_entry(a_directory_name_entry_ptr dnep)
/*
Add dnep to the available list of directory name entries.
*/
{
  dnep->next = avail_directory_name_entries;
  avail_directory_name_entries = dnep;
}  /* free_directory_name_entry */


static a_directory_name_entry_ptr alloc_directory_name_entry(void)
/*
Allocate a new directory name entry, and set its fields to default values.
The space is allocated in general (not IL or FE) memory.
*/
{
  a_directory_name_entry_ptr entry_ptr;

  if (avail_directory_name_entries == NULL) {
    entry_ptr = (a_directory_name_entry_ptr)
                                 alloc_general(sizeof(a_directory_name_entry));
  } else {
    entry_ptr = avail_directory_name_entries;
    avail_directory_name_entries = avail_directory_name_entries->next;
  }  /* if */
  entry_ptr->dir_name = NULL;
  entry_ptr->system_include_dir = FALSE;
  entry_ptr->next     = NULL;
  return (entry_ptr);
}  /* alloc_directory_name_entry */

#if !STANDALONE_UTILITY_PROGRAM

void remove_duplicate_include_dirs(
			a_directory_name_entry_ptr	*include_path_boundary,
			a_boolean			sys_includes_only)
				
/*
Go through the search path and remove any duplicated include directories.
If sys_includes_only is TRUE, only entries that duplicate system include
directories are removed.  If -I- was specified on the command line, the
pointer to which include_path_boundary points will be non-NULL and will
point to the directory immediately preceding the -I- option; this pointer
will be updated appropriately if that directory is removed because of
duplication.
*/
{
  a_directory_name_entry_ptr	dnep1;

  for (dnep1 = incl_search_path; dnep1 != NULL; dnep1 = dnep1->next) {
    /* If this is a system include, or if we are processing all includes,
       look for duplicates of this entry. */
    if (!sys_includes_only || dnep1->system_include_dir) {
      a_directory_name_entry_ptr	dnep2;
      a_directory_name_entry_ptr	prev_dnep2 = NULL;
      a_directory_name_entry_ptr	next_dnep2 = NULL;
      dnep2 = sys_includes_only ? incl_search_path : dnep1->next;
      for (; dnep2 != NULL; dnep2 = next_dnep2) {
        next_dnep2 = dnep2->next;
        if (!sys_includes_only && !dnep1->system_include_dir &&
            *include_path_boundary != NULL &&
            dnep2 == (*include_path_boundary)->next) {
          /* If a -Ixxx is specified on both sides of the boundary, don't
             remove it.  Note the include_file_boundary points to the last
             include directory before the -I-. */
          break;
        }  /* if */
        /* Look for another include directory with the same name. */
        if (dnep1 != dnep2 &&
            (!sys_includes_only || !dnep2->system_include_dir) &&
            compare_dir_names(dnep1->dir_name, dnep2->dir_name,
                             /*is_partial_file_name=*/FALSE) == 0) {
          /* Remove the secondary entry from the list.  For system includes,
             this could be a prior entry. */
          if (prev_dnep2 != NULL) {
            prev_dnep2->next = dnep2->next;
          }  /* if */
          if (incl_search_path == dnep2) {
            incl_search_path = dnep2->next;
          }  /* if */
          if (dnep1->next == dnep2) {
            dnep1->next = dnep2->next;
          }  /* if */
          if (*include_path_boundary == dnep2) {
            /* We removed the directory immediately following -I-: change
               the boundary marker either to the one before that or, if the
               removed directory was the head of the list, to NULL. */
            *include_path_boundary = prev_dnep2;
          }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
          if (db_flag_is_set("incl_search_path")) {
            fprintf(f_debug, "Removing %s, which duplicates a %s incl\n",
                    dnep2->dir_name, sys_includes_only ? "system" : "regular");
            db_incl_search_path();
          }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
          if (sys_includes_only) {
            /* Issue a warning if a directory was specified as both
               a system and non-system include. */
            str_command_line_warning(ec_incl_dir_both_sys_and_nonsys,
                                     dnep2->dir_name);
          }  /* if */
          free_directory_name_entry(dnep2);
          continue;
        }  /* if */
        prev_dnep2 = dnep2;
      }  /* for */
    }  /* if */
  }  /* for */
}  /* remove_duplicate_include_dirs */

#endif /* !STANDALONE_UTILITY_PROGRAM */

void add_to_specified_include_search_path(
			a_const_char			*dir_name,
			a_boolean			system_include_dir,
			a_directory_name_entry_ptr	*search_path,
			a_directory_name_entry_ptr	*end_search_path)
/*
Add the indicated directory to the end of the specified include file search
path.  If system_include_dir is TRUE the directory should be marked as
a "system" include directory.  The directory name string should be
allocated in general memory.
*/
{
  a_directory_name_entry_ptr new_search_path;

  new_search_path = alloc_directory_name_entry();
  new_search_path->dir_name = dir_name;
  new_search_path->system_include_dir = system_include_dir;
  new_search_path->next     = NULL;
  if (*search_path == NULL) {
    *search_path = new_search_path;
  } else {
    (*end_search_path)->next = new_search_path;
  }  /* if */
  *end_search_path = new_search_path;
}  /* add_to_specified_include_search_path */


void add_to_include_search_path(a_const_char	*dir_name,
				a_boolean	system_include_dir)
/*
Add the indicated directory to the end of the include file search
path.  If system_include_dir is TRUE the directory should be marked as
a "system" include directory.
*/
{
  add_to_specified_include_search_path(dir_name, system_include_dir,
                                       &incl_search_path,
                                       &end_incl_search_path);
}  /* add_to_include_search_path */


void add_to_front_of_include_search_path(
				a_const_char		   *dir_name,
				a_directory_name_entry_ptr *search_path,
				a_directory_name_entry_ptr *end_search_path)
/*
Add the indicated directory to the front of the include file search
path.  The directory name string should be allocated in general memory.
*/
{
  a_directory_name_entry_ptr new_search_path;

  new_search_path = alloc_directory_name_entry();
  new_search_path->dir_name = dir_name;
  new_search_path->next     = *search_path;
  if (*search_path == NULL) *end_search_path = new_search_path;
  *search_path = new_search_path;
}  /* add_to_front_of_include_search_path */


void add_default_include_search_path(
				a_directory_name_entry_ptr *search_path,
				a_directory_name_entry_ptr *end_search_path)
/*
Add the list of directories to be used as a default search path for
include files to the end of the search path lists.
*/
{
#if NO_USR_INCLUDE
  /* On some systems, the standard system include directory should not
     be included.  This is usually because the compiler is being used
     as a cross-compiler and all the "system" include files should be
     explicitly included from somewhere else. */
#else /* !NO_USR_INCLUDE */
  a_const_char *usr_include;

  /* Add the default directory to the end of the normal search path. */
  usr_include = getenv("USR_INCLUDE");
  if (usr_include == NULL) usr_include = DEFAULT_USR_INCLUDE;
  add_to_specified_include_search_path(usr_include,
                                       /*system_include_dir=*/TRUE,
                                       search_path, end_search_path);
#endif /* NO_USR_INCLUDE */
#if __VMS__
  /* For VMS, add the current directory to the search path. */
  add_to_front_of_include_search_path("", search_path, end_search_path);
#endif /* __VMS__ */
}  /* add_default_include_search_path */

#if DEBUG
/* GCOVR_EXCL_START */

void db_incl_search_path(void)
/*
Display the include search path, for debugging purposes.
*/
{
  a_directory_name_entry_ptr	dnep = incl_search_path;
  while (dnep != NULL) {
    fprintf(f_debug, "  %s\n", dnep->dir_name);
    dnep = dnep->next;
  }  /* while */
}  /* db_incl_search_path */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

void change_primary_include_search_dir(a_const_char *dir_name)
/*
Replace the directory name in the primary include file search path
entry by "dir_name".  The directory name string should be allocated in
general memory.
*/
{
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("incl_search_path")) {
    fprintf(f_debug,
            "change_primary_include_search_dir: before changing %s to %s\n",
            incl_search_path->dir_name, dir_name);
    db_incl_search_path();
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /*  DEBUG */
  incl_search_path->dir_name = dir_name;
  embed_search_path->dir_name = dir_name;
}  /* change_primary_include_search_dir */


void push_primary_include_search_dir(a_const_char *dir_name,
                                     a_boolean    system_include_dir)
/*
dir_name is the directory of a source file that has just been pushed onto
the input stack.  Adjust the include search path as appropriate, e.g.,
by adding the directory to the front of the search path.  dir_name must
be allocated in general memory.  system_include_dir is TRUE if the updated
entry should be considered a system include directory.

Note that it is not specified within the ANSI C standard or the ARM what the
search rules should be for nested includes.  By default, the search for
nested includes begins in the source directory of the current input file
(not the source directory of the primary input file).  This is the approach
generally taken by C compilers on UNIX systems.  A "stack-model" variation of
this approach (as employed by Microsoft C compilers) follows from setting
stack_referenced_include_directories to TRUE.
*/
{
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("incl_search_path")) {
    fprintf(f_debug, "push_primary_include_search_dir: pushing %s\n",
            dir_name);
    db_incl_search_path();
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /*  DEBUG */
  /* The "-I-" option disables these changes. */
  if (put_dir_of_each_opened_source_file_on_incl_search_path) {
    if (stack_referenced_include_directories) {
      /* The new directory becomes the primary include search directory, but
         the current one remains in the search path. */
      add_to_front_of_include_search_path(dir_name, &incl_search_path,
                                          &end_incl_search_path);
      add_to_front_of_include_search_path(dir_name, &embed_search_path,
                                          &end_embed_search_path);
    } else {
      /* The name in the current primary include search directory (the head of
         list of directory name entries) is simply replaced by dir_name. */
      change_primary_include_search_dir(dir_name);
    }  /* if */
    incl_search_path->system_include_dir = system_include_dir;
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("incl_search_path")) {
    fprintf(f_debug, "push_primary_include_search_dir: after pushing %s\n",
            dir_name);
    db_incl_search_path();
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /*  DEBUG */
}  /* push_primary_include_search_dir */


void pop_primary_include_search_dir(a_const_char *dir_name,
                                    a_boolean    system_include_dir)
/*
The directory name in the primary include file search path should revert to
"dir_name", as the result of popping an include file from the source
input stack.  system_include_dir is TRUE if the original entry should be
considered a system include directory.
*/
{
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("incl_search_path")) {
    fprintf(f_debug, "pop_primary_include_search_dir: popping to %s\n",
            dir_name);
    db_incl_search_path();
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /*  DEBUG */
  /* The "-I-" option disables these changes. */
  if (put_dir_of_each_opened_source_file_on_incl_search_path) {
    if (stack_referenced_include_directories) {
      /* The entry of the current primary include search directory is removed
         from the search path, and the resulting primary include search
         directory will correspond to dir_name. */
      a_directory_name_entry_ptr  dnep;

      dnep = incl_search_path;
      incl_search_path = incl_search_path->next;
      check_assertion(incl_search_path != NULL &&
                      (strcmp(incl_search_path->dir_name, dir_name) == 0));
      free_directory_name_entry(dnep);
      dnep = embed_search_path;
      embed_search_path = embed_search_path->next;
      check_assertion(embed_search_path != NULL &&
                      (strcmp(embed_search_path->dir_name, dir_name) == 0));
      free_directory_name_entry(dnep);
    } else {
      /* The name in the current primary include search directory (the head of
         list of directory name entries) is simply replaced by dir_name. */
      change_primary_include_search_dir(dir_name);
    }  /* if */
    incl_search_path->system_include_dir = system_include_dir;
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("incl_search_path")) {
    fprintf(f_debug, "pop_primary_include_search_dir: after popping to %s\n",
            dir_name);
    db_incl_search_path();
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /*  DEBUG */
}  /* pop_primary_include_search_dir */


void add_to_template_search_path(a_const_char *dir_name)
/*
Add the indicated directory to the end of the template file search
path.  The directory name string should be allocated in general memory.
*/
{
  a_directory_name_entry_ptr dnep;

  dnep = alloc_directory_name_entry();
  dnep->dir_name = dir_name;
  dnep->next     = NULL;
  if (template_search_path == NULL) {
    template_search_path = dnep;
  } else {
    template_search_path_tail->next = dnep;
  }  /* if */
  template_search_path_tail = dnep;
}  /* add_to_template_search_path */


static a_const_char *end_of_directory_name(a_const_char *file_name)
/*
Return a pointer to the end of the directory part of the indicated file
name, or NULL if there is no directory part.
Note that the following must work for FILE_NAME_FOR_STDIN, which is
used to represent stdin; it must return  NULL.
*/
{
  a_const_char *last_slash;
  a_const_char *last_backslash;

  if (strcmp(file_name, FILE_NAME_FOR_STDIN) == 0) {
    /* Special pseudo-name used for stdin; no directory. */
    last_slash = NULL;
  } else {
#if __VMS__
    /* VMS -- Check for "[]" for directory, or ":" for disk or node name. */
    last_slash = mbc_strrchr(file_name, ']');
    if (last_slash == NULL) last_slash = mbc_strrchr(file_name, ':');
#else /* !__VMS__ */
    /* UNIX-like system -- check for last slash. */
    last_slash = mbc_strrchr(file_name, DIRECTORY_SEPARATOR);
    if (backslash_is_also_dir_separator) {
      /* Allow backslash as an alternative to "/". */
      last_backslash = mbc_strrchr(file_name, '\\');
      if (last_slash == NULL || last_backslash > last_slash) {
        last_slash = last_backslash;
      }  /* if */
    }  /* if */
    if (windows_paths_allowed) {
      /* Check for the ":" of a disk name. */
      if (last_slash == NULL && strlen(file_name) >= 2 && file_name[1] == ':'){
        /* Disk name is specified, as in "c:abc". */
        last_slash = file_name+1;
      }  /* if */
    }  /* if */
#endif /* __VMS__ */
  }  /* if */
  return(last_slash);
}  /* end_of_directory_name */


a_const_char *start_of_file_name(a_const_char *file_name)
/*
Return the first character of the file name portion of "file_name"
(i.e., the part after an optional directory name).
*/
{
  a_const_char *result;

  result = end_of_directory_name(file_name);
  /* If there is a directory, use the character after the end of the
     directory name; otherwise, return the file name passed in. */
  result = result == NULL ? file_name : result + 1;
  return result;
}  /* start_of_file_name */


static a_const_char *end_of_base_name(a_const_char *file_name)
/*
Given a simple file name (with no directory name), return a pointer to
the last byte of the file name before the suffix, if any.  Note that
this is the last byte of the name, not the start of the last (possibly
multibyte) character.
*/
{
  a_const_char *last_dot;
  a_const_char *name_end;

  if ((last_dot = mbc_strrchr(file_name, '.')) == NULL) {
    /* No suffix, end of base name is the same as end of file name. */
    name_end = file_name + strlen(file_name) - 1;
  } else {
    /* End of base name is before the suffix. */
    name_end = last_dot - 1;
  }  /* if */
  return name_end;
}  /* end_of_base_name */


a_const_char *suffix_of(a_const_char	*file_name)
/*
Find the suffix of "file_name".  Return a pointer to the beginning
of the suffix.  If the file has no suffix, a pointer to the null-terminator
of the file name is returned.
*/
{
  a_const_char *ptr;

  ptr = end_of_directory_name(file_name);
  if (ptr == NULL) {
    /* There is no directory, use the file name passed in. */
    ptr = file_name;
  } else {
    /* Use the character after the last "/" of the directory name. */
    ptr++;
  }  /* if */
  ptr = end_of_base_name(ptr) + 1;
  return ptr;
}  /* suffix_of */


char *f_directory_of(a_const_char *file_name,
		     a_boolean    in_general_memory)
/*
Return a string that is the directory name for the given file.  If the
file has no explicit directory, return a representation for the current 
directory.  This routine builds an internal list of directory name strings
and attempts to reuse them to avoid allocating the same string over and
over.  The string returned will be allocated in the intermediate language
memory region (when in_general_memory is FALSE) or in general memory.
*/
{
  a_directory_name_entry_ptr	curr_dir_name;
  a_directory_name_entry_ptr	*list_ptr;
  a_const_char			*last_slash;
  sizeof_t			dir_name_length;
  char				*dir_name;

  list_ptr = in_general_memory ? &dir_name_list_general : &dir_name_list_il;
  last_slash = end_of_directory_name(file_name);
  if (last_slash == NULL) {
    /* No directory name, use "" meaning the current directory. */
    dir_name_length = 0;
  } else {
    /* There is a directory name.  Save its length including punctuation. */
    dir_name_length = (size_t)(last_slash - file_name + 1);
  }  /* if */
  /* Look for an existing name on the list that can be reused. */
  /* We assume that a compilation is not going to use a large number of
     include file directories, so we don't need anything fancier than a
     linear linked list here. */
  for (curr_dir_name = *list_ptr;
       curr_dir_name != NULL;
       curr_dir_name = curr_dir_name->next) {
    dir_name = (char *)curr_dir_name->dir_name;
    if (strlen(dir_name) == dir_name_length &&
        strncmp(dir_name, file_name, size_t_arg(dir_name_length)) == 0) {
      goto found_dir_name;
    }  /* if */
  }  /* for */
  /* No reusable name found.  Allocate a copy of the directory name in the
     file-scope IL region. */
#if !STANDALONE_UTILITY_PROGRAM
  dir_name = in_general_memory
                         ? (char *)alloc_general((sizeof_t)(dir_name_length+1))
                         : (char *)alloc_il((sizeof_t)(dir_name_length+1));
#else /* STANDALONE_UTILITY_PROGRAM */
  check_assertion(in_general_memory);
  dir_name = (char *)alloc_general((sizeof_t)(dir_name_length+1));
#endif /* !STANDALONE_UTILITY_PROGRAM */
  if (dir_name_length > 0) {
    (void)memcpy(dir_name, file_name, size_t_arg(dir_name_length));
  }  /* if */
  dir_name[dir_name_length] = '\0';
  /* Put this new name on the list for future reuse. */
  curr_dir_name = alloc_directory_name_entry();
  curr_dir_name->dir_name = dir_name;
  curr_dir_name->next = *list_ptr;
  *list_ptr = curr_dir_name;
found_dir_name:;
  return dir_name;
}  /* f_directory_of */


#if __MICROSOFT_OS__

static sizeof_t truncated_msdos_base_name_length(a_const_char	*str,
						 sizeof_t	base_length,
						 sizeof_t	suffix_length)
/*
If base_length + suffix_length is greater than the maximum file name
length, return the number of characters of base_length that can be used
without exceeding the file name limit.  "str" is the base name, possibly
with some suffix.
*/
{
  sizeof_t	result = base_length;

  if ((base_length + suffix_length) >= __MAXFILE__) {
    result = truncate_length_to_whole_characters(
                                         str, __MAXFILE__ - suffix_length - 1);
  }  /* if */
  return result;
}  /* truncated_msdos_base_name_length */

#endif /* __MICROSOFT_OS__ */


char *derived_name(a_const_char *file_name,
                   a_const_char *suffix)
/*
Return a string that is the base name of file_name with the given suffix
appended.  The string is allocated in general storage, NOT in an
intermediate language memory region, so it must be copied if it is to
be passed to the back end.
*/
{
  a_const_char *last_slash, *last_dot, *name_start, *name_end;
  sizeof_t     der_name_length, suffix_length, base_name_length;
  char         *der_name;

  /* Find the base name by removing the directory and suffix. */
  last_slash = end_of_directory_name(file_name);
  if (last_slash == NULL) {
    /* No directory name, start of file name is start of base name. */
    name_start = file_name;
  } else {
    /* Start of base name is after the directory name. */
    name_start = last_slash + 1;
  }  /* if */
  /* Find suffix, if any. */
  if ((last_dot = mbc_strrchr(name_start, '.')) == NULL) {
    /* No suffix, end of base name is the same as end of file name. */
    name_end = name_start + strlen(name_start) - 1;
  } else {
    /* End of base name is before the suffix. */
    name_end = last_dot - 1;
  }  /* if */
  /* Copy the base name and suffix into the derived name. */
  suffix_length = strlen(suffix);
  base_name_length = (size_t)(name_end - name_start + 1);
#if __MICROSOFT_OS__
  /* Microsoft limits the length of file names.  If the base name and suffix
     would be too long, truncate the base name. */
  base_name_length = truncated_msdos_base_name_length(name_start,
                                                      base_name_length,
                                                      suffix_length);
#endif /* __MICROSOFT_OS__ */
  der_name_length = base_name_length + suffix_length;
  der_name = (char *)alloc_general(der_name_length+1);
  (void)memcpy(der_name, name_start, size_t_arg(base_name_length));
  (void)memcpy(&der_name[base_name_length], suffix, size_t_arg(suffix_length));
  der_name[der_name_length] = '\0';
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 5) {
    fprintf(f_debug, "derived name = \"%s\".\n", der_name);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return(der_name);
}  /* derived_name */


void append_to_path_name(a_text_buffer_ptr	buffer,
			 a_const_char		*name)
/*
Add "name" to the path name in "buffer".
*/
{
  a_boolean need_to_add_slash = FALSE;
  char	separator_char = DIRECTORY_SEPARATOR;

#if __MICROSOFT_OS__
  /* This is done based on __MICROSOFT_OS__ instead of
     BACKSLASH_IS_ALSO_DIR_SEPARATOR so that a "/" separator will be
     preferred on non-Microsoft operating systems. */
  if (mbc_memchr(buffer->buffer, DIRECTORY_SEPARATOR, buffer->size) != NULL) {
    /* The original path uses regular UNIX-style slashes; use one to splice
       the file and path to make it look consistent. */
    separator_char = DIRECTORY_SEPARATOR;
  } else {
    /* The directory name does not have any UNIX-style slashes or has no
       slashes at all.  In either case, on a Microsoft OS use a backslash. */
    separator_char = '\\';
  }  /* if */
#endif /* __MICROSOFT_OS__ */
  remove_null_terminator_from_text_buffer(buffer);
  if (buffer->size > 0) {
    /* The current path name is not empty.  Add a directory separator. */
    /* See if a slash will have to be added between the two. */
    char	last_char = buffer->buffer[buffer->size - 1];
#if __VMS__
    need_to_add_slash = FALSE;
#else /* !__VMS__ */
    need_to_add_slash = (last_char != DIRECTORY_SEPARATOR);
    if (backslash_is_also_dir_separator) {
      /* Under MSDOS, both kinds of slashes need to be checked. */
      need_to_add_slash = need_to_add_slash && (last_char != '\\');
    }  /* if */
#endif /* __VMS__ */
  }  /* if */
  if (need_to_add_slash) {
    /* Add the slash following the directory name. */
    add_char_to_text_buffer(buffer, separator_char);
  }  /* if */
  /* Add the file name to the directory name. */
  add_string_to_text_buffer(buffer, name);
  /* Add a null terminator. */
  add_char_to_text_buffer(buffer, '\0');
}  /* append_to_path_name */


a_text_buffer_ptr combine_dir_and_file_name(
				a_const_char		*dir_name,
				a_const_char		*file_name,
				a_text_buffer_ptr	buffer)
/*
Combine the given directory name and file name to make a full path name,
and return a pointer to it.  If buffer is not NULL, the name is constructed
in buffer, otherwise it is constructed in a default buffer (that will be
overwritten by the next call that uses it).
*/
{
  /* If no buffer was specified by the caller, use a default buffer. */
  if (buffer == NULL) {
    /* Allocate the default buffer the first time that it is needed. */
    if (dir_and_file_buffer == NULL) {
      dir_and_file_buffer = alloc_text_buffer(256);
    }  /* if */
    buffer = dir_and_file_buffer;
  }  /* if */
  /* Clear the buffer. */
  reset_text_buffer(buffer);
  add_string_to_text_buffer(buffer, dir_name);
  /* Add the file name to the directory name. */
  append_to_path_name(buffer, file_name);
  return buffer;
}  /* combine_dir_and_file_name */

#if !STANDALONE_UTILITY_PROGRAM

char *read_line_from_file(FILE *f_file)
/*
Reads a line of input from the file specified by f_file.  Returns a pointer
to a buffer containing the line read, or NULL at end-of-file.  The pointer
returned points to a static buffer that is reused for each call.  This
is used to read auxiliary files such as .ti files and the predefined
macro file.  It is not used when reading source files.
*/
{
  int		ch;
  a_boolean	is_eof = FALSE;

  if (file_read_buffer == NULL) {
    /* Allocate a buffer into which the line is read. */
    file_read_buffer = alloc_text_buffer(1024);
  }  /* if */
  reset_text_buffer(file_read_buffer);
  while (ch = getc(f_file), ch != EOF && ch != '\n') {
    add_char_to_text_buffer(file_read_buffer, (char)ch);
  }  /* while */
  /* Determine whether to return end-of-file (NULL). */
  if (ch == EOF && file_read_buffer->size == 0) {
    is_eof = TRUE;
  } else if (file_read_buffer->size > 0 &&
             file_read_buffer->buffer[file_read_buffer->size-1] == ' ') {
    /* Strip any trailing blanks.  Note the blank test above could be
       looking at the last byte of a multibyte character, but that is
       okay.  The test is only used to rule out the common case where
       the line does not end in a blank before doing the more expensive
       processing below. */
    char	*ptr;
    char	*last_nonblank;
    /* Add a null terminator to mark the end of the buffer for the
       processing below.  This will be repeated below after we find
       the last non-blank. */
    add_char_to_text_buffer(file_read_buffer, '\0');
    /* Find the last non-blank.  This is done from the start of the
       string to correctly handle multibyte characters. */
    last_nonblank = &file_read_buffer->buffer[0];
    for (ptr = last_nonblank; *ptr != '\0'; increment_mbc_ptr(ptr)) {
      if (*ptr != ' ') last_nonblank = ptr;
    }  /* for */
    if (*last_nonblank != '\0') {
      /* If the buffer is not empty (except for blanks) set ptr to just
         after the last nonblank. */
      ptr = last_nonblank;
      increment_mbc_ptr(ptr);
    }  /* if */
    /* Set the position at which to add characters to one past the last
       non blank.  If the buffer is all blanks, this will make the buffer
       empty. */
    set_buffer_position(file_read_buffer, ptr);
  }  /* if */
  /* Terminate string with a null character. */
  add_char_to_text_buffer(file_read_buffer, '\0');
  return (is_eof ? NULL : file_read_buffer->buffer);
}  /* read_line_from_file */


void replace_file_name_suffix(a_const_char	*new_suffix,
                              a_text_buffer_ptr	file_name_buffer)
/*
Replace the suffix of a file name with a specified suffix.  The
replacement is done in place in file_name_buffer, which is expanded if
necessary.  This routine may be called iteratively.
*/
{
#if CHECKING
  sizeof_t curr_file_name_size;
#endif /* CHECKING */
  sizeof_t new_suffix_length;
  char     *suffix_loc;
#define SUFFIX_DELIMITER '.'

  db_enter(5, "replace_file_name_suffix");
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("replace_file_name_suffix")) {
    fprintf(f_debug, "current file_name = \"%s\", new suffix = \"%s\"\n",
            file_name_buffer->buffer, new_suffix);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  /* Determine the size of file_name, excluding the trailing NULL. */
  new_suffix_length = strlen(new_suffix);
#if CHECKING
  curr_file_name_size = file_name_buffer->size - 1;
  check_assertion(curr_file_name_size > 0);
  check_assertion(file_name_buffer->buffer[curr_file_name_size] == '\0');
#endif /* CHECKING */
  suffix_loc = (char *)suffix_of(file_name_buffer->buffer);
  /* Update the buffer to specify that characters should be added
     at the position specified by suffix_loc. */
  set_buffer_position(file_name_buffer, suffix_loc);
  if (new_suffix_length > 0) {
    /* Add the delimiter. */
    add_char_to_text_buffer(file_name_buffer, SUFFIX_DELIMITER);
    /* Add the new suffix to the new filename. */
    add_to_text_buffer(file_name_buffer, new_suffix, new_suffix_length);
  }  /* if */
  /* Terminate the string. */
  add_char_to_text_buffer(file_name_buffer, '\0');
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("replace_file_name_suffix")) {
    fprintf(f_debug, "new file name = \"%s\"\n", file_name_buffer->buffer);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  db_exit();
#undef SUFFIX_DELIMITER
}  /* replace_file_name_suffix */

#endif /* !STANDALONE_UTILITY_PROGRAM */


STATIC_THREAD char
		*current_directory_name;
			/* String containing the front end's current working
			   directory. */


void reset_working_directory()
/*
Reset the current working directory to the process's working directory.
*/
{
  if (current_directory_name != NULL) {
    free_general(current_directory_name, strlen(current_directory_name) + 1);
    current_directory_name = NULL;
  }  /* if */
}  /* reset_working_directory */


void set_working_directory(a_const_char *dir_name)
/*
dir_name is the null-terminated name of the new current working directory
override in the internal encoding.  Update the current working directory value
appropriately.
*/
{
  reset_working_directory();
  current_directory_name = new_copy_of_string(dir_name,
                                              General_allocator<char>());
}  /* set_working_directory */


a_const_char *get_working_directory()
/*
Return a null-terminated string containing the current working directory name.
Note the string returned by this function is deleted by any subsequent calls to
set_working_directory.
*/
{
  if (current_directory_name == NULL) {
    /* Determine the best initial capacity for the buffer. */
#ifdef MAXPATHLEN
#define CWD_BUFFER_INIT_CAP MAXPATHLEN
#else /* !defined(MAXPATHLEN) */
#define CWD_BUFFER_INIT_CAP 256
#endif /* MAXPATHLEN */
    /* Determine the buffer type and define the default "zero" value. */
#if EDG_WIN32 && UNICODE_SOURCE_SUPPORTED
    using a_buffer_type = Dyn_array<wchar_t, General_allocator>;
    constexpr wchar_t zero_elem_value = L'\0';
#else /* !(EDG_WIN32 && UNICODE_SOURCE_SUPPORTED) */
    using a_buffer_type = Dyn_array<char, General_allocator>;
    constexpr char    zero_elem_value = '\0';
#endif /* EDG_WIN32 && UNICODE_SOURCE_SUPPORTED */
    /* Define a buffer for calls to getcwd or getwd. */
    a_buffer_type     cwd_buffer(CWD_BUFFER_INIT_CAP, zero_elem_value);
    a_const_char      *internal_cwd_str;

#if USE_GETCWD
    /* Determine the correct getcwd function. */
#if EDG_WIN32
    using a_cwd_size_arg_type = int;
#if UNICODE_SOURCE_SUPPORTED
#define CWD_FUNC_NAME _wgetcwd
#else /* !UNICODE_SOURCE_SUPPORTED */
#define CWD_FUNC_NAME _getcwd
#endif /* UNICODE_SOURCE_SUPPORTED */
#else /* !EDG_WIN32 */
    using a_cwd_size_arg_type = size_t;
#define CWD_FUNC_NAME getcwd
#endif /* EDG_WIN32 */
    while (TRUE) {
      if (CWD_FUNC_NAME(cwd_buffer.begin(),
                        (a_cwd_size_arg_type)cwd_buffer.length()) == NULL) {
        if (errno == ERANGE) {
          /* We know the buffer is too small, but we don't know how much
             more space we need.  Double the buffer and try again. */
          cwd_buffer.resize(cwd_buffer.length() * 2, zero_elem_value);
          continue;
        }  /* if */
      }  /* if */
      break;
    }  /* while */
#if EDG_WIN32 && UNICODE_SOURCE_SUPPORTED
    internal_cwd_str = conv_wide_to_utf8(cwd_buffer.begin());
#else /* !(EDG_WIN32 && UNICODE_SOURCE_SUPPORTED) */
    internal_cwd_str =
                     (char*)file_name_in_internal_encoding(cwd_buffer.begin());
#endif /* EDG_WIN32 && UNICODE_SOURCE_SUPPORTED */
#else /* !USE_GETCWD */
    (void)getwd(cwd_buffer.begin());
    internal_cwd_str =
                     (char*)file_name_in_internal_encoding(cwd_buffer.begin());
#endif /* USE_GETCWD */
    set_working_directory(internal_cwd_str);
  }  /* if */
  return current_directory_name;
  /* Undefine macros defined in this function. */
#undef CWD_FUNC_NAME
#undef CWD_BUFFER_INIT_CAP
}  /* get_working_directory */


static a_const_char* resolve_file_name(a_const_char *file_name,
                                       a_const_char *dir_name = NULL);


static FILE *fopen_interface(a_const_char *filename,
                             a_const_char *mode)
/*
Interface to the standard fopen routine.  If multibyte characters are
supported in the file name, handle that specially.
*/
{
  FILE         *file;
  a_const_char *abs_file_name = resolve_file_name(filename);
#if EDG_WIN32 && UNICODE_SOURCE_SUPPORTED
  wchar_t      *wide_filename = translate_filename_to_wchar(abs_file_name);

  if (wide_filename != NULL) {
    /* The filename has embedded non-ASCII characters, so we need to use
       the _wfopen routine.  Translate the mode string into wide characters
       as well. */
    wchar_t        wmode[10];
    sizeof_t       wmodelen;
    sizeof_t       i;

    wmodelen = strlen(mode)+1;
    check_assertion(wmodelen <= sizeof(wmode)/sizeof(wmode[0]));
    for (i = 0; i < wmodelen; i++) wmode[i] = (wchar_t)mode[i];
    /* Call the wide open routine. */
    file = _wfopen(wide_filename, wmode);
  } else {
    /* The file name contained no special characters.  Just do a normal
       open. */
    file = fopen(filename, mode);
  }  /* if */
#else /* !(EDG_WIN32 && UNICODE_SOURCE_SUPPORTED) */
  /* Translate the file name into the form used by the file system. */
  filename = file_name_in_external_encoding(abs_file_name);
  file = fopen(filename, mode);
#endif /* EDG_WIN32 && UNICODE_SOURCE_SUPPORTED */
  return file;
}  /* fopen_interface */


char *get_file_modification_time_string(a_const_char	*file_name,
				        a_boolean	strip_newline)
/*
Return the last modification time of "file_name" as a date/time string.
If the file does not exist, or is not a regular file, return NULL.
The ctime function includes a newline in the returned string.  If
strip_newline is TRUE, the newline is replaced with a null terminator.
The string returned is the static buffer returned by the ctime function,
which will be overwritten when ctime is called again.
*/
{
  time_t       mod_time;
  char         *time_str = NULL;
  a_const_char *abs_file_name = resolve_file_name(file_name);

  if (get_file_modification_time(abs_file_name, &mod_time)) {
    time_str = ctime(&mod_time);
    if (strip_newline) {
      char *ptr;
      /* Replace the newline with a null. */
      ptr = (char *)mbc_strchr(time_str, '\n');
      if (ptr != NULL) *ptr = '\0';
    }  /* if */
  }  /* if */
  return time_str;
}  /* get_file_modification_time_string */


size_t get_file_size(a_const_char *file_name)
/*
Return the size in bytes of the file designated by file_name.  The caller
is responsible for ensuring that the file exists.
*/
{
  size_t       sz = 0;
  a_const_char *abs_file_name = resolve_file_name(file_name);
  a_const_char *ext_file_name = file_name_in_external_encoding(abs_file_name);

#if EDG_WIN32
  struct _stat stat_buffer;
  if (_stat(ext_file_name, &stat_buffer) == 0) {
    sz = (size_t)stat_buffer.st_size;
  } else {
    str_catastrophe(ec_cannot_get_file_size, file_name);
  }  /* if */
#else /* !EDG_WIN32 */
  struct stat stat_buffer;
  if (stat(ext_file_name, &stat_buffer) == 0) {
    sz = (size_t)stat_buffer.st_size;
  } else {
    str_catastrophe(ec_cannot_get_file_size, file_name);
  }  /* if */
#endif /* EDG_WIN32 */
  return sz;
}  /* get_file_size */


a_boolean is_regular_file(a_const_char *file_name)
/*
Return TRUE if the specified file is a regular file (i.e., not a
directory or some other kind of special file).
*/
{
  a_const_char *abs_file_name = resolve_file_name(file_name);

  return get_file_modification_time(abs_file_name, (time_t *)NULL);
}  /* is_regular_file */


void clear_open_file_result(an_open_file_result	*open_result)
/*
Clear the fields of an_open_file_result entry.
*/
{
  open_result->flags = 0;
  open_result->errno_value = 0;  
}  /* clear_open_file_result */


#if UNICODE_SOURCE_SUPPORTED

static void do_check_for_byte_order_mark(
                                    FILE                  *f_file,
                                    a_unicode_source_kind *unicode_source_kind,
                                    a_const_char          *file_name)
/*
We are at the start of a source file.  See if the f_file begins with a
byte order mark and return *unicode_source_kind set appropriately
to indicate the kind of encoding used in the file.  Use the value of
default_unicode_source_kind if there is no byte order mark.  file_name
is the name of the file, which is used for diagnostic purposes.
*/
{
  int		ch;
  a_boolean	is_eof;

  /* The byte order marks are:
       EF BB BF   UTF-8
       FF FE      UTF-16 little-endian
       FE FF      UTF-16 big-endian
  */
  *unicode_source_kind = default_unicode_source_kind;
  ch = getc(f_file);
  is_eof = is_eof_char(ch);
  if (!is_eof &&
      (ch != 0xef && ch != 0xff && ch != 0xfe)) {
    /* The first character of the file is not the start of a byte order
       mark.  Unget the character so that it will be fetched when the
       source line is read. */
#if CHECKING
    int ungetc_result =
#else /* !CHECKING */
    (void)
#endif /* CHECKING */
    ungetc(ch, f_file);
    check_assertion(ungetc_result != EOF);
  } else {
    a_boolean is_bom = FALSE;
    /* Read the subsequent characters of the byte order mark.  Stop if
       we hit a character that is not part of the mark. */
    if (!is_eof) {
      int ch2;
      ch2 = getc(f_file);
      if (ch == 0xef && ch2 == 0xbb) {
        /* Possible UTF-8 BOM. */
        ch2 = getc(f_file);
        if (ch2 == 0xbf) {
          is_bom = TRUE;
          *unicode_source_kind = usk_utf8;
        }  /* if */
      } else if (ch == 0xff && ch2 == 0xfe) {
        /* UTF-16 little-endian BOM. */
        is_bom = TRUE;
        *unicode_source_kind = usk_utf16LE;
      } else if (ch == 0xfe && ch2 == 0xff) {
        /* UTF-16 big-endian BOM. */
        is_bom = TRUE;
        *unicode_source_kind = usk_utf16BE;
      }  /* if */
      if (!is_bom) {
        /* The file did not begin with a byte order mark.  Reset the file
           position to the start of the file. */
        if (fseek(f_file, 0L, SEEK_SET) != 0) {
          /* The seek could not be done.  This implies some change
             in the file since last it was opened. */
          an_open_file_result	open_result;
          clear_open_file_result(&open_result);
          file_open_error(es_catastrophe, ec_source, file_name, &open_result);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* do_check_for_byte_order_mark */

#endif /* UNICODE_SOURCE_SUPPORTED */

a_boolean file_exists(a_const_char  *file_name)
/*
Return TRUE if the given file exists; otherwise, return FALSE.
*/
{
  a_boolean    result = FALSE;
  struct stat  stat_buffer;
  a_const_char *abs_file_name = resolve_file_name(file_name);
  a_const_char *ext_file_name = file_name_in_external_encoding(abs_file_name);

  if (stat(ext_file_name, &stat_buffer) == 0) {
    result = TRUE;
  }  /* if */
  return result;
}  /* file_exists */


FILE *fopen_with_result(a_const_char		*file_name,
		        a_const_char		*mode,
		        an_open_file_result	*open_result)
/*
Interface to fopen_interface that maps any resulting error into
an_open_file_result (*open_result).
*/
{
  FILE	*f_result;

  clear_open_file_result(open_result);
  if (strlen(file_name) == 0) {
    open_result->flags |= OFR_BAD_NAME;
    f_result = NULL;
  } else if ((f_result = fopen_interface(file_name, mode)) == NULL) {
    open_result->errno_value = errno;
    /* fopen will return ENOENT for "no such file or directory".  Map this onto
       "not found".  All other errors are mapped onto "cannot open". */
    if (errno == ENOENT) {
      open_result->flags |= OFR_NOT_FOUND;
    } else {
      open_result->flags |= OFR_CANNOT_OPEN;
    }  /* if */
  } else {
    /* File opened okay. */
    /* Check the file type. */
    if (!is_regular_file(file_name)) {
      /* Not a "regular" file. */
      if (is_directory(file_name)) {
        open_result->flags |= OFR_IS_DIRECTORY;
      } else {
        open_result->flags |= OFR_NOT_REGULAR;
      }  /* if */
      (void)fclose(f_result);
      f_result = NULL;
    }  /* if */
  }  /* if */
  return f_result;
}  /* fopen_with_result */


FILE *open_source_file(a_const_char          *file_name,
		       an_open_file_result   *open_result,
                       a_unicode_source_kind *unicode_source_kind)
/*
Open the given file as a source input file, and return a pointer to the
file, or NULL if the file cannot be opened.  In the error case, return
information about the failure in *open_result.  *unicode_source_kind
is set to indicate the Unicode encoding form for the file, or usk_none if the
file is not Unicode.
*/
{
  FILE        *temp_file;

#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("open_source_file") || debug_level >= 2) {
    fprintf(f_debug, "About to open %s\n", file_name);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  *unicode_source_kind = usk_none;
  temp_file = fopen_with_result(file_name, FOPEN_MODE_FOR_READ, open_result);
#if UNICODE_SOURCE_SUPPORTED
  if (temp_file != NULL) {
    /* If the file contains a byte order mark, advance past it. */
    if (check_for_byte_order_mark) {
      do_check_for_byte_order_mark(temp_file, unicode_source_kind,
                                   file_name);
    }  /* if */
  }  /* if */
#endif /* UNICODE_SOURCE_SUPPORTED */
  return(temp_file);
}  /* open_source_file */


FILE *reopen_source_file(a_const_char          *file_name,
                         a_unicode_source_kind *unicode_source_kind)
/*
Reopen the source file of the given name, and return a pointer to
the file block, or NULL if the file cannot be opened.  Since the file has
previously been opened, the open should fail only under unusual and
serious circumstances, such as the file having been deleted during
the compilation.  *unicode_source_kind is set to indicate
the Unicode encoding form for the file, or usk_none if the file is not
Unicode.
*/
{
  FILE	*temp_file;

  temp_file = fopen_interface(file_name, FOPEN_MODE_FOR_READ);
  *unicode_source_kind = usk_none;
#if UNICODE_SOURCE_SUPPORTED
  /* If the file contains a byte order mark, advance past it. */
  if (temp_file != NULL && check_for_byte_order_mark) {
    do_check_for_byte_order_mark(temp_file, unicode_source_kind, file_name);
  }  /* if */
#endif /* UNICODE_SOURCE_SUPPORTED */
  return temp_file;
}  /* reopen_source_file */


a_boolean okay_as_output_file(a_const_char *file_name)
/*
Return TRUE if the given file name is acceptable as an output file.
This involves (potentially) not just checks on the file system permissions,
but checks on whether the file name suffix is something a compiler should be
writing.  This helps avoid problems with clobbering of input files.
*/
{
  a_boolean okay = TRUE;

  if (primary_source_file_name != NULL &&
      strcmp(file_name, primary_source_file_name) == 0) {
    /* Name is the same as the primary source file name, so it's not okay.
       Note, however, that it won't catch cases where the
       source file name and output file name are the same file but
       written in different ways, as for example with different but
       equivalent directory names. */
    okay = FALSE;
  }  /* if */
  return(okay);
}  /* okay_as_output_file */


FILE *open_output_file(a_const_char		*file_name,
                       a_boolean		binary_file,
                       a_boolean		update_mode,
		       an_open_file_result	*open_result)
/*
Open the given file as an output file, and return a pointer to the
file, or NULL if the file cannot be opened.  In the error case,
*open_result indicates the type of error.  binary_file is TRUE if the
file should be opened as a binary file instead of a text file.
update_mode is TRUE if the file should be opened in update mode so it
can be read as well as written.
*/
{
  FILE *temp_file;
  char *mode;

  if (!okay_as_output_file(file_name)) {
    clear_open_file_result(open_result);
    open_result->flags |= OFR_BAD_NAME;
    temp_file = NULL;
  } else {
    if (update_mode) {
      mode = (char *)(binary_file ? FOPEN_MODE_FOR_BINARY_UPDATE :
                                    FOPEN_MODE_FOR_UPDATE);
    } else {
      mode = (char *)(binary_file ? FOPEN_MODE_FOR_BINARY_WRITE
                                  : FOPEN_MODE_FOR_WRITE);
    }  /* if */
    temp_file = fopen_with_result(file_name, mode, open_result);
  }  /* if */
  return temp_file;
}  /* open_output_file */


a_boolean close_output_file(FILE	*f_output,
			    int		*errno_value)
/*
Check for errors in writing the output file, then close it.  If an error
occurred return TRUE.  Set *errno_value to the errno of the operation that
caused the error.
*/
{
  a_boolean has_error = FALSE;

  *errno_value = 0;
  if (f_output != NULL) {
    if (fflush(f_output)) {
      *errno_value = errno;
      has_error = TRUE;
    }  /* if */
    if (ferror(f_output)) {
      *errno_value = errno;
      has_error = TRUE;
    }  /* if */
    if (f_output != stdout) {
      if (fclose(f_output) && !has_error) {
        *errno_value = errno;
        has_error = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return has_error;
}  /* close_output_file */


FILE *open_input_file(a_const_char		*file_name,
                      a_boolean			binary_file,
		      an_open_file_result	*open_result)
/*
Open the given file as an input file, and return a pointer to the
file, or NULL if the file cannot be opened.  binary_file is TRUE if
the file should be opened as a binary file instead of a text file.
If the file can be opened and is a regular file, return the FILE pointer,
otherwise return NULL.  If the open fails, *open_result indicates the
reason for the failure.
*/
{
  FILE *temp_file;
  char *mode;

#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 2) {
    fprintf(f_debug, "About to open input file %s\n", file_name);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  mode = (char *)(binary_file ? FOPEN_MODE_FOR_BINARY_READ :
                                FOPEN_MODE_FOR_READ);
  /* Open the file. */
  temp_file = fopen_with_result(file_name, mode, open_result);
  return(temp_file);
}  /* open_input_file */


#if __VMS__
extern "C" int delete(char *file_name);
#endif /* __VMS__ */

void delete_file(a_const_char *file_name)
/*
Delete the file with the indicated name.  It shouldn't be open currently.
*/
{
  int          status;
  a_const_char *abs_file_name = resolve_file_name(file_name);

  errno = 0;
#if EDG_WIN32 && UNICODE_SOURCE_SUPPORTED
  { wchar_t *wide_file_name = translate_filename_to_wchar(abs_file_name);

    if (wide_file_name != NULL) {
      /* The filename has embedded non-ASCII characters, so we need to use
         the _wremove routine. */
      status = _wremove(wide_file_name);
    } else {
      /* The file name contained no special characters. */
      status = remove(abs_file_name);
    }  /* if */
  }
#else /* !(EDG_WIN32 && UNICODE_SOURCE_SUPPORTED) */
  /* Translate the file name into the form used by the file system. */
  a_const_char *ext_file_name = file_name_in_external_encoding(abs_file_name);
#if __ANSIC__
  status = remove(ext_file_name);
#else /* __ANSIC__ */
#if __VMS__
  status = delete(ext_file_name);
#else /* !__VMS__ */
  status = unlink(ext_file_name);
#endif /* __VMS__ */
#endif /* __ANSIC__ */
#endif /* EDG_WIN32 && UNICODE_SOURCE_SUPPORTED */
  if (status != 0) {
    str_errno_catastrophe(ec_file_delete_error_reason, file_name, errno);
  }  /* if */
}  /* delete_file */


#if __MICROSOFT_OS__
/*
Data structure used to keep a list of open temporary files under MS-DOS,
in order to get their names to close them.
*/
typedef struct a_temp_file_name *a_temp_file_name_ptr;
typedef struct a_temp_file_name {
  a_temp_file_name_ptr
		next;	/* Next entry on the list, or NULL if this is the
			   last entry. */
  char		*name;	/* Null-terminated file name string. */
  FILE		*file;	/* File pointer. */
} a_temp_file_name;

STATIC_THREAD a_temp_file_name_ptr
		open_temp_files;
			/* List of all temp files currently open. */
#endif /* __MICROSOFT_OS__ */

STATIC_THREAD a_const_char
                *temp_dir;
                        /* The cached temporary directory name. */

STATIC_THREAD size_t
                temp_dir_len;
                        /* The length of the cached temporary directory
                           name (temp_dir). */

constexpr int   temp_file_tries = 20;
                        /* The maximum number of tries when opening a temporary
                           file. */

using a_temp_file_name_buffer = Small_string<150>;
                        /* The type used for a temporary file name buffer. */

#if EDG_WIN32 && USE_HOST_TMPFILE_FACILITIES

static inline a_string get_temp_dir_from_win32()
/*
Return a string containing the temporary directory name using the Win32 API.
*/
{
  a_string result;
#if UNICODE_SOURCE_SUPPORTED
  typedef DWORD (WINAPI *GTP2W)(DWORD, LPWSTR);
  wchar_t  buffer[MAX_PATH];
  DWORD    path_len;
  GTP2W    new_tmp_path_func = (GTP2W)GetProcAddress(
                                             GetModuleHandleW(L"kernel32.dll"),
                                             "GetTempPath2W");

  if (new_tmp_path_func != NULL) {
    /* GetTempPath2W is present in this kernel version: make use of it. */
    path_len = new_tmp_path_func(MAX_PATH, buffer);
  } else {
    path_len = GetTempPathW(MAX_PATH, buffer);
  }  /* if */
#else /* !UNICODE_SOURCE_SUPPORTED */
  typedef DWORD (WINAPI *GTP2A)(DWORD, LPSTR);
  char     buffer[MAX_PATH];
  DWORD    path_len;
  GTP2A    new_tmp_path_func = (GTP2A)GetProcAddress(
                                              GetModuleHandleA("kernel32.dll"),
                                              "GetTempPath2A");

  if (new_tmp_path_func != NULL) {
    /* GetTempPath2A is present in this kernel version: make use of it. */
    path_len = new_tmp_path_func(MAX_PATH, buffer);
  } else {
    path_len = GetTempPathA(MAX_PATH, buffer);
  }  /* if */
#endif /* UNICODE_SOURCE_SUPPORTED */
  if (path_len > (MAX_PATH - 14) || path_len == 0) {
    /* GetTempPath failed to retrieve a valid temporary path; fall back to the
       binary's default temporary directory.  Note that the Win32 API specifies
       that GetTempFileName fails if the string is longer than MAX_PATH - 14
       characters. */
    result.reset_to(DEFAULT_TMPDIR);
  } else {
    /* Use the path from GetTempPath (converting from utf-16 to utf-8 if
       necessary). */
#if UNICODE_SOURCE_SUPPORTED
    result.reset_to(conv_wide_to_utf8(buffer));
#else /* !UNICODE_SOURCE_SUPPORTED */
    result.reset_to(file_name_in_internal_encoding(buffer));
#endif /* UNICODE_SOURCE_SUPPORTED */
  }  /* if */
  return result;
}  /* get_temp_dir_from_win32 */

#else /* !(EDG_WIN32 && USE_HOST_TMPFILE_FACILITIES) */

static inline a_string get_temp_dir_from_env()
/*
Return a string containing the temporary directory name by querying common
environment variables.
*/
{
  a_string result;
  char     *temp_dir_env = NULL;

#if __MICROSOFT_OS__
  /* On a Microsoft OS, first use the TMP environment variable, if set. */
#if EDG_WIN32 && UNICODE_SOURCE_SUPPORTED
  wchar_t *wide_temp_dir_env = _wgetenv(L"TMP");

  if (wide_temp_dir_env != NULL) {
    temp_dir_env = conv_wide_to_utf8(wide_temp_dir_env);
  }  /* if */
#else /* !(EDG_WIN32 && UNICODE_SOURCE_SUPPORTED) */
  temp_dir_env = getenv("TMP");
#endif /* EDG_WIN32 && UNICODE_SOURCE_SUPPORTED */
#else /* !__MICROSOFT_OS__ */
  /* On a Unix OS, use the TMPDIR environment variable, if set. */
  temp_dir_env = getenv("TMPDIR");
#endif /* __MICROSOFT_OS__ */
  if (temp_dir_env == NULL || strlen(temp_dir_env) == 0) {
    /* Fall back to the default temporary directory name (one could not be
       found in the environment variable). */
    result.reset_to(DEFAULT_TMPDIR);
  } else {
    /* Use the temporary directory from the environment variable. */
#if EDG_WIN32 && UNICODE_SOURCE_SUPPORTED
    result.reset_to(temp_dir_env);
#else /* !(EDG_WIN32 && UNICODE_SOURCE_SUPPORTED) */
    result.reset_to(file_name_in_internal_encoding(temp_dir_env));
#endif /* EDG_WIN32 && UNICODE_SOURCE_SUPPORTED */
  }  /* if */
  return result;
}  /* get_temp_dir_from_env */

#endif /* EDG_WIN32 && USE_HOST_TMPFILE_FACILITIES */

static inline void resolve_temp_dir()
/*
Initialize temp_dir if not already initialized.
*/
{
  /* Get the value of the "TMPDIR" environment variable, the directory to
     be used for temporary files.  Get it only once (temp_dir is static). */
  if (temp_dir == NULL) {
    /* If this a relatively modern Windows system, use the Win32 API to
       determine the correct temporary path directory. */
    a_string tmp_buffer =
#if EDG_WIN32 && USE_HOST_TMPFILE_FACILITIES
                          get_temp_dir_from_win32();
#else /* !(EDG_WIN32 && USE_HOST_TMPFILE_FACILITIES) */
                          get_temp_dir_from_env();
#endif /* EDG_WIN32 && USE_HOST_TMPFILE_FACILITIES */

    /* Ensure there's a directory separator on the end of the path. */
    a_boolean need_slash = FALSE;
    if (!tmp_buffer.is_empty()) {
      char last_char = tmp_buffer[tmp_buffer.length() - 1];

      if (last_char != DIRECTORY_SEPARATOR) {
        need_slash = TRUE;
      }  /* if */
#if __MICROSOFT_OS__
      /* Under MS-DOS we don't need to add a slash if the path already ends
         with a backslash. */
      if (need_slash && last_char == '\\') {
        need_slash = FALSE;
      }  /* if */
#endif /* __MICROSOFT_OS__ */
    }  /* if */
    if (need_slash) {
      tmp_buffer.append(DIRECTORY_SEPARATOR_STRING);
    }  /* if */
    /* Move the computed temporary directory name to general memory for use
       during the rest of the compilation. */
    temp_dir = tmp_buffer.to_allocated_storage(General_allocator<char>());
    temp_dir_len = tmp_buffer.length();
  }  /* if */
}  /* resolve_temp_dir */

#if __MICROSOFT_OS__

static inline void register_temp_file_for_removal(
                                      const a_temp_file_name_buffer &file_name,
                                      FILE                          *file)
/*
On Windows temporary files cannot be unlinked from the file system ahead of
time; instead, add the file to a list of files to be cleaned up later.
*/
{
  /* Use general storage for the allocation, because it may have to survive
     into a back end called in the same program as the front end. */
  a_temp_file_name_ptr new_entry =
                 (a_temp_file_name_ptr)alloc_general(sizeof(a_temp_file_name));

  new_entry->name = file_name.to_allocated_storage(General_allocator<char>());
  new_entry->file = file;
  new_entry->next = open_temp_files;
  open_temp_files = new_entry;
}  /* register_temp_file_for_removal */

#endif /* __MICROSOFT_OS__ */
#if USE_HOST_TMPFILE_FACILITIES
#if EDG_WIN32

static inline a_temp_file_name_buffer get_win32_temp_file_name()
/*
Return a file name for a new temporary file.  If a temporary file name
cannot be acquired, instead return an empty string.
*/
{
  a_temp_file_name_buffer result;
#if UNICODE_SOURCE_SUPPORTED
  wchar_t                 *wide_temp_dir =
                                         translate_filename_to_wchar(temp_dir);

  if (wide_temp_dir != NULL) {
    wchar_t  win_buffer[MAX_PATH];
    unsigned path_found = GetTempFileNameW(wide_temp_dir,
                                           L"edg",
                                           0,
                                           win_buffer);
    if (path_found == 0) {
      /* A suitable file name was not discovered; return nothing. */
      goto done;
    }  /* if */
    /* Convert back to UTF-8. */
    result.reset_to(conv_wide_to_utf8(win_buffer));
  } else
#endif /* UNICODE_SOURCE_SUPPORTED */
  /* Do not add code here. */
  {
    char     win_buffer[MAX_PATH];
    unsigned path_found = GetTempFileNameA(temp_dir,
                                           "edg",
                                           0,
                                           win_buffer);
    if (path_found == 0) {
      /* A suitable file name was not discovered; return nothing. */
      goto done;
    }  /* if */
    /* Move the result to the primary buffer. */
    result.reset_to(win_buffer);
  }  /* if */
done:
  return result;
}  /* get_win32_temp_file_name */


static inline FILE *open_win32_temp_file(a_boolean binary_file)
/*
Open a temporary file using the operating system's temporary file creation
methods, and return a pointer to its file block.  The created file is opened in
binary mode if binary_file is TRUE; otherwise, the file is opened textually.
*/
{
  FILE                    *result = NULL;
  a_temp_file_name_buffer file_name;

  for (int try_number = 0; try_number < temp_file_tries; ++try_number) {
    file_name = get_win32_temp_file_name();
    if (file_name.is_empty()) {
      continue;
    }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
    if (debug_level >= 4) {
      fprintf(f_debug, "Opening temporary file %s\n",
              file_name.as_temp_characters());
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

    /* A temporary file path has been computed, attempt to open the file. */
    char *mode = (char *)(binary_file ? FOPEN_MODE_FOR_BINARY_UPDATE
                                      : FOPEN_MODE_FOR_UPDATE);
    result = fopen_interface(file_name.as_temp_characters(), mode);
    if (result != NULL) goto have_file;
  }  /* for */
  output_file_open_error(/*bad_name=*/FALSE, ec_temporary,
                         file_name.as_temp_characters(),
                         es_catastrophe);
  /* This function does not return from the above call. */
have_file:;
  /* The file cannot be deleted now, so add it to the list of files to be
     cleaned up. */
  register_temp_file_for_removal(file_name, result);
  return result;
}  /* open_win32_temp_file */

#else /* !EDG_WIN32 */

static inline FILE *open_unix_temp_file(a_boolean binary_file)
/*
Open a temporary file using the operating system's secure temporary file
creation strategy, and return a pointer to its file block.  The created file is
opened in binary mode if binary_file is TRUE; otherwise, the file is opened
textually.  The file is unlinked from its associated file system path
immediately after creation and will be destroyed upon close of the returned
file.
*/
{
  FILE                    *result;
  constexpr a_const_char  *pattern_str = "edg.XXXXXX";
  size_t                  pattern_str_len = strlen(pattern_str);
  a_temp_file_name_buffer file_name(a_string_view(temp_dir, temp_dir_len));
  int                     tmp_fd = -1;

  for (int try_number = 0; try_number < temp_file_tries; ++try_number) {
    /* Append the template pattern (this is reset between loop iterations as
       mkstemp may have replaced it with a concrete file name on the last
       try). */
    file_name.append(a_string_view(pattern_str, pattern_str_len));
    /* Versions of glibc before 2.7 (released in 2007) incorrectly used 0666
       permissions for mkstemp.  POSIX.1-2008 adds a requirement that the file
       be created with mode 0600.  Coverity suggests using a umask in this
       situation to avoid the possibility that mode 0666 is successfully
       applied.  This adds significant application complexity particularly for
       multi-threaded builds as umask is a process level setting.  Given the
       age of the problem (at the time of writing nearly two decades) and the
       implementation complexity to take the Coverity suggestion, the warning
       is thus suppressed. */
    /* coverity[secure_temp] */
    tmp_fd = mkstemp((char*)file_name.as_temp_characters());
    if (tmp_fd != -1) {
      break;
    }  /* if */
    file_name.truncate_to(temp_dir_len);
  }  /* for */
  if (tmp_fd == -1) {
    /* The template pattern has been truncated when leaving the retry loop
       above; add the template back for the error. */
    file_name.append(a_string_view(pattern_str, pattern_str_len));
    output_file_open_error(/*bad_name=*/FALSE, ec_temporary,
                           file_name.as_temp_characters(),
                           es_catastrophe);
    /* This function does not return from the above call. */
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4) {
    fprintf(f_debug, "Opened temporary file %s\n",
            file_name.as_temp_characters());
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

  /* Create a FILE from the file descriptor. */
  char *mode = (char *)(binary_file ? FOPEN_MODE_FOR_BINARY_UPDATE
                                    : FOPEN_MODE_FOR_UPDATE);
  result = fdopen(tmp_fd, mode);
  /* Delete the file now, so it will disappear when closed. */
  (void)unlink(file_name.as_temp_characters());
  return result;
}  /* open_unix_temp_file */

#endif /* EDG_WIN32 */
#else /* !USE_HOST_TMPFILE_FACILITIES */

STATIC_THREAD unsigned long
                temp_seed;
                        /* A seed value to reduce collisions when using
                           open_legacy_temp_file. */


static FILE *open_legacy_temp_file(a_boolean binary_file)
/*
Open a temporary file using an EDG algorithm to compute a temporary file name,
and return a pointer to its file block.  The created file is opened in binary
mode if binary_file is TRUE; otherwise, the file is opened textually.
*/
{
  FILE                    *result = NULL;
  a_temp_file_name_buffer file_name;

  for (int try_number = 0; try_number < temp_file_tries; ++try_number) {
    /* Put together the name dir + "/edg" + seed + "_" + process id. */
    file_name.reset_to(a_string_view(temp_dir, temp_dir_len),
                       "edg", temp_seed++, "_", (long)getpid());
#if DEBUG
/* GCOVR_EXCL_START */
    if (debug_level >= 4) {
      fprintf(f_debug, "Opening temporary file %s\n",
              file_name.as_temp_characters());
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

    /* Check to see if the file exists already.  If so, go on to the next
       seed value. */
    struct stat buf;
    if (stat(file_name.as_temp_characters(), &buf) == 0) {
      /* The file exists already. */
    } else {
      /* The file does not exist.  Try opening it. */
      char *mode = (char *)(binary_file ? FOPEN_MODE_FOR_BINARY_UPDATE
                                        : FOPEN_MODE_FOR_UPDATE);

      result = fopen_interface(file_name.as_temp_characters(), mode);
      if (result != NULL) goto have_file;
    }  /* if */
    /* Retry with incremented file names a certain number of times.  After
       that, give up (the problem may be that the directory name is bad). */
  }  /* for */
  output_file_open_error(/*bad_name=*/FALSE, ec_temporary,
                         file_name.as_temp_characters(),
                         es_catastrophe);
  /* This function does not return from the above call. */
have_file:;
#if __MICROSOFT_OS__
  /* The file cannot be deleted now, so add it to the list of files to be
     cleaned up. */
  register_temp_file_for_removal(file_name, result);
#else /* !__MICROSOFT_OS__ */
  /* Delete the file now, so it will disappear when closed. */
  (void)unlink(file_name.as_temp_characters());
#endif /* __MICROSOFT_OS__ */
  return result;
}  /* open_legacy_temp_file */

#endif /* USE_HOST_TMPFILE_FACILITIES */

FILE *open_temp_file(a_boolean binary_file)
/*
Open a temporary file, and return a pointer to its file block.  The created
file is opened in binary mode if binary_file is TRUE; otherwise, the file is
opened textually.
*/
{
  FILE *result;

  /* Resolve the temporary directory if not already resolved (temp_dir). */
  resolve_temp_dir();
  /* If this assertion fails, resolve_temp_dir failed to correctly populate
     the temp_dir (and needs to be corrected). */
  check_assertion(temp_dir != NULL && strlen(temp_dir) > 0 &&
                  strlen(temp_dir) == temp_dir_len);
  /* Use the configured strategy to open the temporary file in the resolved
     temporary directory. */
#if USE_HOST_TMPFILE_FACILITIES
#if EDG_WIN32
  result = open_win32_temp_file(binary_file);
#else /* !EDG_WIN32 */
  result = open_unix_temp_file(binary_file);
#endif /* EDG_WIN32 */
#else /* !USE_HOST_TMPFILE_FACILITIES */
  result = open_legacy_temp_file(binary_file);
#endif /* USE_HOST_TMPFILE_FACILITIES */
  return result;
}  /* open_temp_file */

#if MAKE_FRONT_END_CALLABLE

void close_file_if_open(FILE	**f_file)
/*
Close the file specified by *f_file and set the file pointer to NULL.
*/
{
  if (*f_file != NULL) {
    (void)fclose(*f_file);
    *f_file = NULL;
  }  /* if */
}  /* close_file_if_open */

#endif /* MAKE_FRONT_END_CALLABLE */

void close_temp_file(FILE *temp_file)
/*
Close and delete the indicated temporary file.
*/
{
  (void)fclose(temp_file);
#if __MICROSOFT_OS__
  { a_temp_file_name_ptr tfnp, prev_tfnp;
    /* Find the entry for this file on the list of open files. */
    for (prev_tfnp = NULL, tfnp = open_temp_files;
         tfnp != NULL;
         prev_tfnp = tfnp, tfnp = tfnp->next) {
      if (tfnp->file == temp_file) {
        /* Found it; delete the file and remove the entry from the list.
           The space for the entry is just lost, but that's not a big issue. */
        (void)delete_file(tfnp->name);
        if (prev_tfnp == NULL) {
          /* First entry on list. */
          open_temp_files = tfnp->next;
        } else {
          prev_tfnp->next = tfnp->next;
        }  /* if */
        goto close_done;
      }  /* if */
    }  /* for */
  }
#if CHECKING
  internal_error("close_temp_file: file not on list");
#endif /* CHECKING */
close_done:;
#else /* !__MICROSOFT_OS__ */
  /* The file was unlinked when opened, and therefore was deleted automatically
     when closed. */
#endif /* __MICROSOFT_OS__ */
}  /* close_temp_file */

#if __MICROSOFT_OS__

static void close_all_temp_files(void)
/*
Close and delete all open temporary files.
*/
{
  while (open_temp_files != NULL) {
    close_temp_file(open_temp_files->file);
  }  /* while */
}  /* close_all_temp_files */

#endif /* __MICROSOFT_OS__ */
#if COMPILE_MULTIPLE_SOURCE_FILES

void identify_source_file(void)
/*
Identify the source file being compiled, when more than one file name
appears on the command line.
*/
{
#if !USING_DRIVER
  if (more_than_one_source_file) {
    fprintf(f_error, "%s:\n", format_file_name(primary_source_file_name));
  }  /* if */
#endif /* !USING_DRIVER */
}  /* identify_source_file */

#endif /* COMPILE_MULTIPLE_SOURCE_FILES */
#if !STANDALONE_UTILITY_PROGRAM

static void write_sarif_init()
/*
Write the "prelude" for the SARIF output.  When this function exits, the front
end is expected to output SARIF result objects to f_error.
*/
{
  /* Print the SARIF "prelude", i.e.:

     {
       "version": "2.1.0",
       "$schema": "https://raw.githubusercontent.com/oasis-tcs/[etc...]",
       "runs": [
         {
           "tool": {
             "driver": {
               "name": "EDG CPFE",
               "version": "[version]",
               "organization": "Edison Design Group",
               "fullName": "Edison Design Group C/C++ Front End - [version]",
               "informationUri": "https://edg.com/c"
             }
           },
           "columnKind": "unicodeCodePoints",
           "results": [
   */
  fprintf(f_error,
          "{\"version\":\"2.1.0\",\"$schema\":\""
          "https://raw.githubusercontent.com/oasis-tcs/sarif-spec/master/"
          "Schemata/sarif-schema-2.1.0.json\",\"runs\":[{"
          "\"tool\":{\"driver\":{"
          "\"name\":\"EDG CPFE\","
          "\"version\":\"%s\","
          "\"organization\":\"Edison Design Group\","
          "\"fullName\":\"Edison Design Group C/C++ Front End - %s\","
          "\"informationUri\":\"https://edg.com/c\"}},"
          "\"columnKind\":\"unicodeCodePoints\","
          "\"results\":[", VERSION_NUMBER, VERSION_NUMBER);
}  /* write_sarif_init */

#endif /* !STANDALONE_UTILITY_PROGRAM */

void write_init()
/*
Write the "prelude" for the front end's execution.
*/
{
#if !STANDALONE_UTILITY_PROGRAM
  switch (output_mode) {
    case om_text:
      /* No init needs to occur here. */
      break;
    case om_sarif:
      write_sarif_init();
      break;
    default_is_unexpected();
  }  /* switch */
#endif /* !STANDALONE_UTILITY_PROGRAM */
}  /* write_init */

#if STANDALONE_UTILITY_PROGRAM

NORETURN void normal_termination(void)
/*
Terminate a utility program normally.  This is used by the C-generating back 
end and il_display when they are compiled as standalone programs.
This routine does not return.
*/
{
  exit(RC_NORMAL);
}  /* normal_termination */

#else /* !STANDALONE_UTILITY_PROGRAM */

static void write_text_signoff()
/*
Write a compilation signoff message, giving the count of errors.
Only write the signoff if there ARE errors, and if we are supposed to.
*/
{
#if WRITE_SIGNOFF_MESSAGE && !STANDALONE_UTILITY_PROGRAM
  a_diagnostic_counter diags_total = diagnostic_counters.total;

#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("module_report") && num_module_decls_attempted > 0) {
    fprintf(f_debug, "%lu modules declarations processed (%lu failed).\n",
            num_module_decls_attempted,
            num_module_decls_failed);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  if (diags_total.all_error_types() > 0) {
    a_diagnostic_counter diags_suppressed = diagnostic_counters.suppressed;
    unsigned long        total_diags_reported = diags_total.all_error_types() -
                                            diags_suppressed.all_error_types();

    /* Generate the "X errors and Y catastrophic errors" portion of the
       diagnostic.  Only generate this if there were reported diagnostics;
       otherwise, simplify the message to only include the suppressed
       diagnostics portion. */
    if (total_diags_reported > 0) {
      if (diags_total.errors > 0) {
        fprintf(f_error, "%lu %s", diags_total.errors,
                error_text(diags_total.errors != 1 ? ec_wrapup_errors
                                                   : ec_wrapup_error));
        if (diags_total.catastrophes > 0) {
          fprintf(f_error, " %s ", error_text(ec_and));
        }  /* if */
      }  /* if */
      if (diags_total.catastrophes > 0) {
        fprintf(f_error, "%lu %s", diags_total.catastrophes,
                error_text(diags_total.catastrophes != 1
                                              ? ec_wrapup_catastrophic_errors
                                              : ec_wrapup_catastrophic_error));
      }  /* if */
    }  /* if */
    /* Generate the "(including K suppressed errors)" or (when all diagnostics
       encountered were suppressed) the simplified "K suppressed errors"
       portion of the message. */
    if (diags_suppressed.all_error_types() > 0) {
      /* Catastrophes should never be suppressed. */
      check_assertion(diags_suppressed.catastrophes == 0);
      if (total_diags_reported > 0) {
        fprintf(f_error, " (%s ", error_text(ec_including));
      }  /* if */
      if (diags_total.errors > 0) {
        fprintf(f_error, "%lu %s %s", diags_suppressed.errors,
                error_text(ec_suppressed),
                error_text(diags_suppressed.errors != 1 ? ec_wrapup_errors
                                                        : ec_wrapup_error));
      }  /* if */
      if (total_diags_reported > 0) {
        fputs(")", f_error);
      }  /* if */
    }
    fputs(" ", f_error);
    /* Generate the "in the compilation of 'Z'." portion of the diagnostic. */
    if (primary_source_file_name != NULL &&
        strlen(primary_source_file_name) != 0 &&
        strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) != 0) {
      fprintf(f_error, error_text(ec_det_in_compilation_of),
              format_file_name(primary_source_file_name));
    } else {
      /* Source file name is not known. */
      fputs(error_text(ec_det_in_compilation), f_error);
    }  /* if */
    fputs("\n", f_error);
  }  /* if */
#endif /* WRITE_SIGNOFF_MESSAGE && !STANDALONE_UTILITY_PROGRAM */
}  /* write_text_signoff */


static void write_sarif_signoff()
/*
Write the "epilogue" for the SARIF output.  This terminates the SARIF output,
starting at the SARIF results array.
*/
{
  fputs("]}]}\n", f_error);
}  /* write_sarif_signoff */

#endif /* STANDALONE_UTILITY_PROGRAM */

void write_signoff()
/*
Write the "epilogue" for the front end's execution.
*/
{
#if !STANDALONE_UTILITY_PROGRAM
  switch (output_mode) {
    case om_text:
      write_text_signoff();
      break;
    case om_sarif:
      write_sarif_signoff();
      break;
    default_is_unexpected();
  }  /* switch */
#endif /* !STANDALONE_UTILITY_PROGRAM */
}  /* write_signoff */

#if MAKE_FRONT_END_CALLABLE

STATIC_THREAD a_boolean
		signal_caught = FALSE;
			/* Set to TRUE if a signal has been caught.  We
			   should really exit the compilation in such cases. */

#endif /* MAKE_FRONT_END_CALLABLE */

NORETURN static void cfe_exit(int status)
/*
This is a wrapper around the exit function.  Normally, it just calls
the exit routine, but when the front end is callable this routine
saves the return status and transfers control back to the top-level
routine of the front end, so that it can return to the caller.
*/
{
#if !MAKE_FRONT_END_CALLABLE
  exit(status);
#else /* MAKE_FRONT_END_CALLABLE */
  /* If a signal was caught, actually exit the compilation. */
  if (signal_caught) {
    exit(status);
  } else {
    exit_status = status;
    longjmp(edg_main_setjmp_buffer, 1);
  }  /* if */
#endif /* !MAKE_FRONT_END_CALLABLE */
}  /* cfe_exit */


NORETURN void exit_compilation(an_error_severity severity)
/*
Exit the compilation.  severity indicates the severity of the most
severe diagnostic issued in this compilation.  This routine does not return.
*/
{
#if !USING_DRIVER && !STANDALONE_UTILITY_PROGRAM
  if (output_mode == om_text) {
    /* For the more serious severities, write a message about the abrupt
       termination. */
    if (severity == es_catastrophe || severity == es_command_line_error) {
      fprintf(f_error, "Compilation terminated.\n");
    } else if (severity == es_internal_error) {
      fprintf(f_error, "Compilation aborted.\n");
    }  /* if */
  }  /* if */
#endif /* !USING_DRIVER && !STANDALONE_UTILITY_PROGRAM */

  /* Actually terminate the compilation with the proper return code. */
  switch (severity) {
    case es_none:
    case es_remark:
      cfe_exit(RC_NORMAL);
      break;
    case es_warning:
      cfe_exit(RC_WARNING);
      break;
    case es_error:
      cfe_exit(RC_ERROR);
      break;
    case es_catastrophe:
    case es_command_line_error:
      cfe_exit(RC_CATASTROPHE);
      break;
    case es_internal_error:
    default:
#if EXIT_ON_INTERNAL_ERROR
      cfe_exit(RC_CATASTROPHE);
      break;
#else /* !EXIT_ON_INTERNAL_ERROR */
      (void)fflush(f_error);
      abort();
#endif /* EXIT_ON_INTERNAL_ERROR */
  }  /* switch */
}  /* exit_compilation */

#if !CHECKING

NORETURN void exit_unrecoverable_compilation()
/*
Exit the compilation with an internal error if an unreachable condition is
encountered.  This routine does not return.
*/
{
  exit_compilation(es_internal_error);
}  /* exit_unrecoverable_compilation */

#endif /* !CHECKING */

NORETURN void term_compilation(an_error_severity severity)
/*
Terminate the compilation.  This is always called as the last thing in the
compilation.  severity indicates the error severity.  This routine does
not return.
*/
{
  /* Write a signoff message (with count of errors) if necessary. */
  write_signoff();
  /* Exit from the compilation. */
  exit_compilation(severity);
}  /* term_compilation */

/*
The front end's signal handlers are designed for single-threaded processes.  In
multi-threaded builds it's expected that signal handlers be installed where
appropriate by the function/thread in charge of managing spawned threads.
*/
#if !MULTIPLE_THREAD_COMPILATION && !EDG_FRONT_END_DAEMON

/*
In C++, signal handlers must be extern "C".
*/
extern "C" {
/*lint -e2761 -e2765*/

NORETURN static a_signal_handler_return_value term_on_signal(
                                                            ARG_UNUSED int sig)
/*
Routine set up as a signal handler, called to terminate compilation on
receipt of a signal.
*/
{
#if !USING_DRIVER
  /* Print newline to make console output clean. */
  fprintf(f_error, "\n");
#endif /* !USING_DRIVER */
#if MAKE_FRONT_END_CALLABLE
  signal_caught = TRUE;
#endif /* MAKE_FRONT_END_CALLABLE */
  term_compilation(es_catastrophe);
}  /* term_on_signal */

#if DEBUG && !EDG_WIN32

NORETURN static a_signal_handler_return_value abort_on_cpu_limit(
                                                            ARG_UNUSED int sig)
/*
Routine set up as a signal handler, called to terminate compilation 
with an internal error on receipt of a signal.
*/
{
#if !USING_DRIVER
  /* Print newline to make console output clean. */
  fprintf(f_error, "\n");
#endif /* !USING_DRIVER */
  fprintf(f_error, "Internal error: CPU time limit exceeded.\n");
  /* Exit from the compilation.  Don't use term_compiilation because it may
     do some problematic operations (such as allocating memory). */
  exit_compilation(es_internal_error);
}  /* abort_on_cpu_limit */

#endif /* DEBUG && !EDG_WIN32 */

/*
The Sun C++ compiler requires a ... as the second parameter of the
signal handler function.  Define a type to which the real signal handler
function pointer will be cast.
*/
#if defined(__SUNPRO_CC) && __BSD__
typedef a_signal_handler_return_value a_signal_handler(int p, ...);
#else /* !(defined(__SUNPRO_CC) && __BSD__) */
/* Standard type for a signal handler. */
typedef a_signal_handler_return_value a_signal_handler(int p);
#endif /* defined(__SUNPRO_CC) && __BSD__ */

}  /* extern "C" */

#if USE_SIGACTION_FOR_SEGV_FAULT_INFO

extern "C" {

static void segv_handler(int        num,
                         siginfo_t* info,
                         void*      context)
/*
Handler invoked for segmentation violations.  See the sigaction system
call for information about the parameters.  This version of the routine
only works on 32-bit x86 Linux systems.
*/
{
  ucontext_t* cp = (ucontext_t*)context;
  unsigned long ip = cp->uc_mcontext.gregs[REG_EIP];
  fprintf(stderr, "Internal error: segmentation fault at %lx\n", ip);
  term_compilation(es_internal_error);
}  /* segv_handler */

}  /* extern "C" */

static void set_segv_handler(void)
/*
Initialize a special signal handler for segmentation violations using
the sigaction facility.  Note that the sigaction system call is only available
on certain systems.  This facility is intended to be used to provide
additional information for debugging purposes.
*/
{
  struct sigaction action;

  memset(&action, 0, sizeof(action));
  action.sa_sigaction = segv_handler;
  sigfillset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &action, 0); 
}  /* set_segv_handler */

#endif /* USE_SIGACTION_FOR_SEGV_FAULT_INFO */
/*lint +e2761 +e2765*/

static void set_signal_handlers(void)
/*
Enable any signal handlers necessary to catch signals that may come up during
execution of the front end (for example, SIGINT).
*/
{
  if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
    /* Only reset the signal if it is not already being ignored.  This is
       to prevent a compilation in the background from being terminated by
       an interrupt intended for the foreground process on older Unix
       systems that lack job control. */
    (void)signal(SIGINT, (a_signal_handler *)term_on_signal);
  }  /* if */
  (void)signal(SIGTERM, (a_signal_handler *)term_on_signal);
#ifdef SIGXFSZ
  /* On SVR4 systems, ignore the signal sent when the file size limit
     is exceeded.  Note that the write operation will still fail, so the
     error will be reported where the file is written. */
  (void)signal(SIGXFSZ, SIG_IGN);
#endif /* SIGXFSZ */
#if DEBUG && !EDG_WIN32
  /* Catch the signal that the CPU limit has been exceeded. */
  (void)signal(SIGXCPU, (a_signal_handler *)abort_on_cpu_limit);
#endif /* DEBUG && !EDG_WIN32 */
#if USE_SIGACTION_FOR_SEGV_FAULT_INFO
  set_segv_handler();
#endif /* USE_SIGACTION_FOR_SEGV_FAULT_INFO */
#if __MICROSOFT_OS__
  /* Under MS-DOS, establish an atexit routine to close and delete all
     temporary files. */
  if (atexit(close_all_temp_files) != 0) {
#if CHECKING
    internal_error("set_signal_handlers: could not set atexit handler");
#endif /* CHECKING */
  }  /* if */
#endif /* __MICROSOFT_OS__ */
}  /* set_signal_handlers */

#endif /* !MULTIPLE_THREAD_COMPILATION && !EDG_FRONT_END_DAEMON */
#ifdef NEED_SIZE_T_ARG_ERROR

/* The extern declaration for this routine is in basics.h. */
NORETURN true_size_t size_t_arg_error(void)
/*
Called from the macro size_t_arg to abort the front end if a size is being
truncated.  This is used on machines that have a small size_t (say, 16 bits)
when we choose to make sizeof_t something longer.
*/
{
  catastrophe(ec_program_too_large);
  /* The return statement suppresses warnings from compilers that don't
     process lint comments. */
  return 0;
}  /* size_t_arg_error */

#endif /* ifdef NEED_SIZE_T_ARG_ERROR */


int smemcmp(a_const_char *s1,
            a_const_char *s2,
            sizeof_t     length)
/*
Like the standard memcmp, but guaranteed to compare the characters
sequentially and read no more characters than necessary.  Used when one
does not know that both strings are at least as long as the indicated length.
*/
{
  int cmp = 0;

  for (; length > 0; length--) {
    cmp = ((unsigned char)(*s1++) - (unsigned char)(*s2++));
    if (cmp != 0) break;
  }  /* for */
  return cmp;
}  /* smemcmp */

#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED

void set_multibyte_locale(void)
/*
If appropriate, set the locale to allow processing of multibyte characters
in source.  Only change the category of processing related to character
handling functions.  The locale is only set the first time this function
is called.
*/
{
  if (!locale_already_set) {
#if !USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING && \
    !EDG_MULTIBYTE_CHAR_TEST_MODE && \
    !(UNICODE_SOURCE_SUPPORTED && \
      !defined(LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED))
    if (setlocale(LC_CTYPE,
                  LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED) == NULL) {
      str_catastrophe(ec_bad_multibyte_char_locale,
                      LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED);
    }  /* if */
#endif /* !USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING && ... */
    locale_already_set = TRUE;
  }  /* if */
}  /* set_multibyte_locale */

#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

static a_cpu_time get_cpu_time(void)
/*
Returns the amount of CPU time used by this process (and any child processes)
since the start of the compilation.  The value is converted to milliseconds.
*/
{
#if __ANSIC__
  /* This version of the routine uses the ANSI/ISO C compliant version of the
     clock function. */
  clock_t	cpu_time;
  double	temp;

  cpu_time = clock();
  /* clock() returns a value in units of CLOCKS_PER_SEC.  Convert this value
     to milliseconds. */
  temp = (double)cpu_time;
  temp = (temp * 1000) / CLOCKS_PER_SEC;
  cpu_time = (clock_t)temp;
  return (a_cpu_time)cpu_time;
#else /* !__ANSIC__ */
  /* This version uses the UNIX routines to get the CPU time. */
  clock_t	cpu_time = 0;
  double	temp;
  struct tms	buffer;

  if (times(&buffer) != -1) {
    cpu_time = buffer.tms_utime + buffer.tms_stime +
               buffer.tms_cutime + buffer.tms_cstime;
    /* times() returns a value in unspecified units.  CLOCK_FREQUENCY should
       be defined by host_envir.h to the appropriate value.  Convert this
       value to milliseconds. */
    temp = cpu_time;
    temp = (cpu_time * 1000) / CLOCK_FREQUENCY;
    cpu_time = temp;
  }  /* if */
  return cpu_time;
#endif /* __ANSIC__ */
}  /* get_cpu_time */


static a_real_time get_time(void)
/*
Return the current wall clock time normalized to seconds.
*/
{
  return ((a_real_time)time((time_t*)NULL));
}  /* get_time */


void get_timer(a_timer	*timer)
/*
Gets the current CPU time and wall clock time and returns the result
to the caller.
*/
{
  timer->cpu_time = get_cpu_time();
  timer->real_time = get_time();
}  /* get_timer */

static void calc_time_difference(a_timer_ptr	start_time,
			         a_timer_ptr	end_time,
			         double		*cpu_time,
			         double		*real_time)
/*
Given a starting and ending timer, return the elapsed time and CPU
time in seconds.
*/
{
#if __ANSIC__
  /* If available, use the ANSI routine to compute the difference between
     the two real times. */
  *real_time = difftime((time_t)end_time->real_time,
                        (time_t)start_time->real_time);
#else /* !__ANSIC__ */
  /* The real times are assumed to be stored in seconds. */
  *real_time = end_time->real_time - start_time->real_time;
#endif /* __ANSIC__ */
  *cpu_time = ((double)(end_time->cpu_time) -
              ((double)start_time->cpu_time)) / 1000;
}  /* calc_time_difference */


void display_time_used(a_const_char	*message,
		       a_timer_ptr	start_time,
		       a_timer_ptr	end_time)
/*
Display the difference in CPU time and elapsed time between two timers.
*/
{
  double	cpu_time;
  double	real_time;

  calc_time_difference(start_time, end_time, &cpu_time, &real_time);
  fprintf(f_error, "%-30s %10.2f (CPU) %10.2f (elapsed)\n", message,
          cpu_time, real_time);
}  /* display_time_used */


static a_const_char *convert_file_name_encoding(
                                              a_const_char         *orig_name,
                                              ARG_UNUSED a_boolean to_internal)
/*
orig_name is the null-terminated name of a file or directory.  Translate
to or from the internal encoding of the file name (depending on the value
of to_internal).  If some translation is required, a new string is allocated
in general storage, it is filled with the converted form, and its address
is returned. If no conversion is required, the original string is returned.
*/
{
  char *file_name = (char *)orig_name;

#if UNICODE_SOURCE_SUPPORTED
#if EDG_WIN32 && NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
  /* In case the native multibyte locale has been changed (e.g., by the
     setlocale pragma) set it back to the system default locale for purposes
     of file name translation. */
  _locale_t	saved_locale = native_multibyte_locale;
  native_multibyte_locale = system_default_locale;
#endif /* !(EDG_WIN32 && NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE) */
  if (DEFAULT_UNICODE_SOURCE_KIND == usk_none) {  /*lint !e506*/
    /* The environment uses a non-Unicode encoding.  Go through the file
       name and check for any characters that must be converted to or
       from Unicode (depending on to_internal).  If it contains any such
       characters, a new copy of the string must be created. */
    a_boolean     conversion_needed = FALSE;
    sizeof_t      size_needed = 0;
    a_const_char  *p;
    unsigned long wc;
    int           in_len;
    sizeof_t      out_len;
    unsigned char arr[4];
    /* Look to see whether the string contains any characters that
       require conversion.  Also determine the size needed if we have to
       allocate space for the converted copy. */
    for (p = orig_name; *p != '\0'; p += in_len) {
      in_len = mbc_to_wide_char(p, &wc, (a_boolean*)NULL,
                                /*is_native=*/to_internal);
      /* A conversion is needed if the input was a multibyte character or
         if we are converting to internal form and the input character must
         be converted to UTF-8. */
      if (in_len == 1 && (!to_internal || wc <= 0x7f)) {
        out_len = 1;
      } else {
        conversion_needed = TRUE;
        out_len = to_internal ? unicode_to_utf8(wc, arr) : 1;
      }  /* if */
      size_needed += out_len;
    }  /* for */
    if (conversion_needed) {
      /* The string contains at least one character that needs to be rewritten.
         Allocate and fill a new string. */
      char *dest = alloc_general(size_needed+1);
      file_name = dest;
      for (p = orig_name; *p != '\0'; p += in_len) {
        a_boolean err;

        in_len = mbc_to_wide_char(p, &wc, &err, /*is_native=*/to_internal);
        /* If the character could not be converted, substitute a "?". */
        if (err) wc = (unsigned long)'?';
        if (to_internal) {
          /* Converting from the external encoding to UTF-8. */
          if (wc <= 0x7f && in_len == 1) {
            *dest++ = *p;
          } else {
            out_len = unicode_to_utf8(wc, arr);
            for (sizeof_t i = 0; i < out_len; i++) *dest++ = (char)arr[i];
          }  /* if */
        } else {
          /* Converting from UTF-8 to the external encoding.  This version
             only supports Latin-1. */
          if (wc <= UCHAR_MAX) {
            *dest++ = (char)wc;
          } else {
            /* The character does not fit in a single byte. Substitute
               a "?". */
            wc = (unsigned long)'?';
          }  /* if */
        }  /* if */
      }  /* for */
      *dest = '\0';
    }  /* if */
  } else {
    /* We don't have code to handle UTF-16 as the default character set
       from the environment. */
    check_assertion(DEFAULT_UNICODE_SOURCE_KIND == usk_utf8); /*lint !e506*/
  }  /* if */
#if EDG_WIN32 && NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
  /* Restore the original locale. */
  native_multibyte_locale = saved_locale;
#endif /* !(EDG_WIN32 && NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE) */
#endif /* UNICODE_SOURCE_SUPPORTED */
  return file_name;
}  /* convert_file_name_encoding */


a_const_char *file_name_in_internal_encoding(a_const_char *orig_name)
/*
orig_name is the null-terminated name of a file or directory as provided
by the environment, e.g., from the command line or from a system call.
Convert it if necessary to the character encoding used internally
for file names.  If some translation is required, a new string is allocated
in general storage, it is filled with the converted form, and its address
is returned. If no conversion is required, the original string is returned.
*/
{
  a_const_char *file_name;

  file_name = convert_file_name_encoding(orig_name, /*to_internal=*/TRUE);
  return file_name;
}  /* file_name_in_internal_encoding */


a_const_char *file_name_in_external_encoding(a_const_char *orig_name)
/*
orig_name is the null-terminated name of a file or directory in the
internal encoding.  Convert it if necessary to the form used by the
environment.  If some translation is required, a new string is allocated
in general storage, it is filled with the converted form, and its address
is returned. If no conversion is required, the original string is returned.
*/
{
  a_const_char *file_name;

  file_name = convert_file_name_encoding(orig_name, /*to_internal=*/FALSE);
  return file_name;
}  /* file_name_in_external_encoding */


/* Header comment for is_directory */
/*
Determine whether the specified path name is the name of a valid
directory.
*/
#if EDG_WIN32
/*
WIN32 (e.g., Windows-NT) version.
*/
a_boolean is_directory(a_const_char *file_name)
{
  a_boolean    result = FALSE;
  DWORD        attr;
  a_const_char *abs_file_name = resolve_file_name(file_name);
#if UNICODE_SOURCE_SUPPORTED
  wchar_t      *wide_file_name = translate_filename_to_wchar(abs_file_name);

  if (wide_file_name != NULL) {
    /* The file name has embedded non-ASCII characters, so we need to use
       the GetFileAttributesW routine. */
    /* Call the wide routine. */
    attr = GetFileAttributesW(wide_file_name);
  } else {
    /* The file name contained no special characters. */
    attr = GetFileAttributes(abs_file_name);
  }  /* if */
#else /* !UNICODE_SOURCE_SUPPORTED */
  /* Translate the file name into the form used by the file system. */
  a_const_char *ext_file_name = file_name_in_external_encoding(abs_file_name);
  attr = GetFileAttributes(ext_file_name);
#endif /* UNICODE_SOURCE_SUPPORTED */
  result = attr != INVALID_FILE_ATTRIBUTES &&
           (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
  return result;
}  /* is_directory */
/* Define a flag that indicates that a definition of is_directory has
   been supplied. */
#define IS_DIRECTORY_DEFINED

#else /* !EDG_WIN32 */
#if defined(S_ISDIR) || defined(S_IFDIR)
/*
UNIX Version.

When the macro S_ISDIR is defined, we assume that the stat system call
is available and can be used to determine whether a file name is
a directory.
*/
a_boolean is_directory(a_const_char *file_name)
{
  a_boolean    result = FALSE;
  a_const_char *abs_file_name = resolve_file_name(file_name);
  a_const_char *ext_file_name = file_name_in_external_encoding(abs_file_name);
  struct stat  buf;

  /* Check the file type.  Use the stat call instead of fstat because some
     implementations do not have the _file field in the structure. */
  if (stat(ext_file_name, &buf) == 0) {
    /* Use the POSIX S_ISDIR if it is defined.  Otherwise use the
       non-POSIX test using S_IFDIR. */
#ifdef S_ISDIR
    result = S_ISDIR(buf.st_mode);
#else /* ifndef S_ISDIR */
    result = ((buf.st_mode & S_IFDIR) != 0);
#endif /* ifdef S_ISDIR */
  }  /* if */
  return result;
}  /* is_directory */

/* Define a flag that indicates that a definition of is_directory has
   been supplied. */
#define IS_DIRECTORY_DEFINED

#endif /* defined(S_ISDIR) || defined(S_IFDIR) */
#endif /* EDG_WIN32 */
#ifndef IS_DIRECTORY_DEFINED

#if MULTIPLE_THREAD_COMPILATION
 #error -- The fallback is_directory implementation is incompatible with \
           MULTIPLE_THREAD_COMPILATION
#endif /* MULTIPLE_THREAD_COMPILATION */

static void chdir_with_check(a_const_char *dir_name)
/*
Change to the specified directory and make sure the operation succeeded.
*/
{
  a_boolean	failed = FALSE;
#if EDG_WIN32 && UNICODE_SOURCE_SUPPORTED
  wchar_t *wide_dir_name = translate_filename_to_wchar(dir_name);

  if (wide_dir_name != NULL) {
    /* The directory name has embedded non-ASCII characters, so we need to use
       the _wchdir routine. */
    failed = _wchdir(wide_dir_name) != 0;
  } else {
    /* The file name contained no special characters.  Just do a normal
       chdir. */
    failed = chdir(dir_name) != 0;
  }  /* if */
#else /* !(EDG_WIN32 && UNICODE_SOURCE_SUPPORTED) */
  /* Translate the file name into the form used by the file system. */
  dir_name = file_name_in_external_encoding(dir_name);
  failed = chdir(dir_name) != 0;
#endif /* EDG_WIN32 && UNICODE_SOURCE_SUPPORTED */
  if (failed) {
    str_catastrophe(ec_cannot_chdir, dir_name);
  }  /* if */
}  /* chdir_with_check */


a_boolean is_directory(a_const_char *file_name)
/*
This is a portable version of is_directory that should work on any
system that supports chdir.
*/
{
  a_boolean  result = FALSE;

  /* Try to change to the specified directory. */
  if (chdir(file_name) == 0) {
    /* The operation succeeded. */
    result = TRUE;
  }  /* if */
  /* Return to the original directory. */
  chdir_with_check(get_working_directory());
  return result;
}  /* is_directory */

#endif /* ifndef IS_DIRECTORY_DEFINED */


static inline a_boolean is_dir_separator(a_const_char ch)
/*
Return TRUE if "ch" is a directory separator character, FALSE otherwise.
*/
{
  return (backslash_is_also_dir_separator && ch == '\\') ||
         ch == DIRECTORY_SEPARATOR;
}  /* is_dir_separator */


static a_boolean has_drive_specification(a_const_char *file_name)
/*
Test whether or not a file name includes a drive specification.  A drive
specification is normally something like "X:" but for UNC file names
the prefix of "\\" is treated as a drive specification.
*/
{
  a_boolean	result;

  result = (isalpha((unsigned char)(file_name)[0]) &&
            ((file_name)[1] == ':')) ||
           (file_name[0] == '\\' && file_name[1] == '\\');
  return result;
}  /* has_drive_specification */


a_boolean is_absolute_file_name(a_const_char *file_name)
/*
Test whether or not a file name is absolute (a full path name).
*/
{
  return (backslash_is_also_dir_separator && (file_name)[0] == '\\') ||
         (windows_paths_allowed && has_drive_specification(file_name)) ||
         (file_name)[0] == DIRECTORY_SEPARATOR;
}  /* is_absolute_file_name */


a_const_char *get_base_name(a_const_char *file_name)
/*
Given a file_name, return a pointer to the start of the base name.  No copying
is performed - if the base name is desired to be kept separate, the caller must
perform the copy.  If the final character is a directory separator, return an
empty string.
*/
{
  a_const_char *result;

  if (windows_paths_allowed && has_drive_specification(file_name)) {
    file_name += 2;
  }  /* if */
  result = file_name;
  while (*file_name != '\0') {
    if (is_dir_separator(*file_name++)) {
      result = file_name;
    }  /* if */
  }  /* if */
  return result;
}  /* get_base_name */


/* Header comment for get_file_name_from_dir. */
/*
Get the name of the next file in the current directory that has the
indicated suffix.  If first is TRUE, then this is the first call.
Returns a pointer to the file name string.  The value returned is
only valid until the next call of this routine.  A NULL pointer is
returned when there are no more directory entries.  dir_name indicates
the directory in which to search (or NULL for the current directory),
and curr_dir_name is the current directory name.
*/
#if EDG_WIN32
/*
WIN32 (e.g., Windows-NT) version.
*/


END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <tchar.h>
#include <dos.h>
#include <io.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */

char *get_file_name_from_dir(a_boolean	             first,
			     a_const_char            *dir_name,
			     a_const_char            *suffix,
			     ARG_UNUSED a_const_char *curr_dir_name)
{
  char                           *result;
  STATIC_THREAD HANDLE           handle;
  STATIC_THREAD WIN32_FIND_DATAA find_file_data;
#if UNICODE_SOURCE_SUPPORTED
  STATIC_THREAD WIN32_FIND_DATAW wide_find_file_data;
  STATIC_THREAD a_boolean        using_wide_char;
#endif /* UNICODE_SOURCE_SUPPORTED */

  if (first) {
    /* Convert the suffix (e.g., ".xxx" into a pattern for use by the
       Windows-NT routine (e.g., "*.xxx"). */
    check_assertion(strlen(suffix) <= 8);

    /* On the first call, use the FindFirstFile call that specifies which files
       are to be returned.  "handle" is saved in a static variable that is used
       on subsequent calls to get the remaining directory entries. */
    Small_string<10> tmp_pattern("*", suffix);
    a_const_char     *resolved_name = resolve_file_name(
                                              tmp_pattern.as_temp_characters(),
                                              dir_name);
#if UNICODE_SOURCE_SUPPORTED
    wchar_t          *wide_name = translate_filename_to_wchar(resolved_name);
    using_wide_char = FALSE;
    if (wide_name != NULL) {
      /* The directory name has embedded non-ASCII characters, so we need to
         use the wide character version of the FindFirstFileW routine. */
      handle = FindFirstFileW(wide_name, &wide_find_file_data);
      using_wide_char = TRUE;
    } else
#endif /* UNICODE_SOURCE_SUPPORTED */
    /* Do not add code here. */
    {
      /* The file name contained no special characters.  Just use the normal
         FindFirstFileA function. */
      handle = FindFirstFileA(resolved_name, &find_file_data);
    }
    if (handle == INVALID_HANDLE_VALUE) {
      /* Directory could not be opened, or is empty. */
      result = NULL;
    } else {
#if UNICODE_SOURCE_SUPPORTED
      if (using_wide_char) {
        result = conv_wide_to_utf8(wide_find_file_data.cFileName);
      } else
#endif /* UNICODE_SOURCE_SUPPORTED */
      /* Do not add code here. */
      {
        result = find_file_data.cFileName;
      }  /* if */
    }  /* if */
  } else {
    /* On subsequent calls, use FindNextFileA to find the next file that
       matches the pattern. */
    int find_result;

#if UNICODE_SOURCE_SUPPORTED
    if (using_wide_char) {
      find_result = FindNextFileW(handle, &wide_find_file_data);
    } else
#endif /* UNICODE_SOURCE_SUPPORTED */
    /* Do not add code here. */
    {
      find_result = FindNextFileA(handle, &find_file_data);
    }  /* if */
    if (find_result == 0) {
      /* Returns 0 when there are no more files. */
      result = NULL;
      /* Release the handle used to read the directory. */
      FindClose(handle);
    } else {
#if UNICODE_SOURCE_SUPPORTED
      if (using_wide_char) {
        result = conv_wide_to_utf8(wide_find_file_data.cFileName);
      } else
#endif /* UNICODE_SOURCE_SUPPORTED */
      /* Do not add code here. */
      {
        result = find_file_data.cFileName;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* get_file_name_from_dir */
#else /* !EDG_WIN32 */
#if EDG_MSDOS
/*
DOS version.
*/

END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <dos.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */

#if MULTIPLE_THREAD_COMPILATION
 #error -- The DOS get_file_name_from_dir implementation is incompatible with \
           MULTIPLE_THREAD_COMPILATION
#endif /* MULTIPLE_THREAD_COMPILATION */

char *get_file_name_from_dir(a_boolean	first,
			     char	*dir_name,
			     char	*suffix,
			     char	*curr_dir_name)
/*
See comment above.
*/
{
  STATIC_THREAD struct _find_t fileinfo;
  char                         *result;
  STATIC_THREAD char           pattern[10];

  if (dir_name != NULL) {
    chdir_with_check(dir_name);
  }  /* if */
  if (first) {
    /* Convert the suffix (e.g., ".xxx" into a pattern for use by the
       find-first routine (e.g., "*.xxx"). */
    check_assertion(strlen(suffix) <= 8);

    /* On the first call, use the _dos_findfirst call that specifies which
       files are to be returned.  The _A_RDONLY attribute causes
       both normal and read-only files to be returned. */
    Small_string<10> tmp_pattern("*", suffix);
    tmp_pattern.write_to_buffer(pattern, /*buffer_len=*/10);
    if (_dos_findfirst(pattern, _A_RDONLY, &fileinfo) != 0) {
      /* Directory could not be opened, or is empty. */
      result = NULL;
    } else {
      result = fileinfo.name;
    }  /* if */
  } else {
    /* On subsequent calls, use _dos_findnext to find the next file that
       matches the pattern. */
    if (_dos_findnext(&fileinfo) != 0) {
      /* Returns non-zero when there are no more files. */
      result = NULL;
    } else {
      result = fileinfo.name;
    }  /* if */
  }  /* if */
  if (dir_name != NULL) {
    chdir_with_check(curr_dir_name);
  }  /* if */
  return result;
}  /* get_file_name_from_dir */
#else /* !EDG_MSDOS */

#if __VMS__
/*
VMS version.  A VMS specific version is not supplied in the standard
distribution.
*/
char *get_file_name_from_dir(ARG_UNUSED a_boolean first,
                             ARG_UNUSED char      *dir_name,
                             ARG_UNUSED char      *suffix,
                             ARG_UNUSED char      *curr_dir_name)
/*
See comment above.

A VMS specific version is not supplied.  The VMS version (but without
automatic PCH support) can be used by getting rid of the #error directive
below.
*/
{
 #error -- a VMS specific version of get_file_name_from_dir must be supplied
  return (char *)NULL;
}  /* get_file_name_from_dir */
#else /* !__VMS__ */

/*
UNIX Version.
*/
#include <sys/types.h>
#include <dirent.h>
#ifndef __AIX__
#ifndef __osf__
#ifndef __linux__
/* This file should not be included on IBM AIX, Digital UNIX, or Linux. */
#include <sys/dirent.h>
#endif /* ifndef __linux__ */
#endif /* ifndef __osf__ */
#endif /* ifndef __AIX__ */

char *get_file_name_from_dir(a_boolean	             first,
                             a_const_char            *dir_name,
                             a_const_char            *suffix,
                             ARG_UNUSED a_const_char *curr_dir_name)
/*
See comment above.
*/
{
  STATIC_THREAD DIR	*dir;
  STATIC_THREAD struct dirent
                        *dir_entry;
  char			*result;

  if (first) {
    /* Open the directory and save the directory pointer in a static
       variable that can be used on subsequent calls. */
    /* If no directory name was specified, use the current directory. */
    if (dir_name == NULL) dir_name = ".";
    dir = opendir(dir_name);
    check_assertion(dir != NULL);
  }  /* if */
  for (;;) {
    a_const_char *ptr;
    dir_entry = readdir(dir);
    if (dir_entry == NULL) {
      /* The last entry was read. */
      (void)closedir(dir);
      result = NULL;
      break;
    }  /* if */
    result = dir_entry->d_name;
    /* Make sure the suffix matches the value passed by the caller. */
    ptr = mbc_strrchr(result, '.');
    if (ptr != NULL && strcmp(ptr, suffix) == 0) break;
  }  /* for */
  return result;
}  /* get_file_name_from_dir */
#endif /* !__VMS__ */
#endif /* EDG_MSDOS */
#endif /* EDG_WIN32 */

a_boolean terminal_is_color_capable(void)
/*
Returns TRUE if the underlying file descriptor is open to a "terminal" device
(e.g., not a file or a pipe) and that it appears that the device is color
capable.
*/
{
  a_boolean result = TRUE;
#if __MICROSOFT_OS__
  if (f_error == NULL || !_isatty(_fileno(f_error))) {
    result = FALSE;
  } else {
    /* Set virtual terminal processing to enable handling the escape sequences
       on Windows. */
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    DWORD dwMode = 0;
    if (hErr == INVALID_HANDLE_VALUE ||
        !GetConsoleMode(hErr, &dwMode) ||
        !SetConsoleMode(hErr, (dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))) {
      result = FALSE;
    }  /* if */
  }  /* if */
#else /* !__MICROSOFT_OS__ */
  if (f_error == NULL || !isatty(fileno(f_error))) {
    result = FALSE;
  } else {
    /* Require that the TERM environment variable is set (and not "dumb"). */
    a_const_char *term = getenv("TERM");
    if (term == NULL || strcmp(term, "dumb") == 0) {
      colorize_diagnostics = FALSE;
    }  /* if */
  }  /* if */
#endif /* __MICROSOFT_OS__ */
  return result;
}  /* terminal_is_color_capable */

#if MODULE_ID_NEEDED

/*
Routines used by lower_init.c and c_gen_be.c to generate module IDs
used to create unique external names.
*/

void change_non_id_characters(char *str)
/*
Change any non-identifier characters in the indicated string to underscores.
*/
{
  for (; *str != '\0'; str++) if (!isalnum((unsigned char)*str)) *str = '_';
}  /* change_non_id_characters */


STATIC_THREAD a_const_char
		*module_id;
			/* A string used to qualify static names that are put
			   out as external names to make them unique.  Note
			   that this variable is not saved/restored when
			   precompiled header files are used; if the entity
			   used to create the module id is defined in a
			   PCH, a NULL value will cause the module id to be
			   re-computed based upon the current translation
			   unit's name (and not the translation unit name
			   that was used when the PCH was created). */

void set_module_id(a_const_char *new_module_id)
/*
Set the module id for the current translation unit to the value specified
by new_module_id.
*/
{
  /* Make sure a module id has not already been assigned. */
  check_assertion(module_id == NULL);
  module_id = new_module_id;
}  /* set_module_id */


a_const_char *get_module_id(void)
/*
Return the module id.
*/
{
  return module_id;
}  /* get_module_id */


a_const_char *make_module_id(a_const_char *external_name)
/*
Make a string that is based on the name of the current module and is used to
qualify static names that are put out as external names, to make them unique.
external_name is the mangled name of an external variable or routine name
defined in this translation unit (or NULL if no such definition exists).
Set module_id to the string and return it.
*/
{
  a_const_char    *file_name;
  sizeof_t        file_name_len;
  a_const_char    *str1;
  a_const_char    *str2;
  Small_string<9> crc_buf;

  /* Only generate the module id the first time that this routine is called
     for a given translation unit. */
  if (module_id == NULL) {
    if (in_front_end) {
      /* In the front end proper, we may be using multiple translation units.
         In that case, it is necessary to use the source file associated
         with the current translation unit. */
      file_name = curr_translation_unit->source_file->file_name;
    } else {
      /* In the back end, there is no longer a concept of multiple translation
         units. */
      file_name = il_header.primary_source_file->file_name;
    }  /* if */
    if (external_name == NULL) {
      /* In the very unlikely event that the file does not define any
         externally visible variables or routines, use the modification
         time of the source file and the current directory name.  If
         there is no source file, use the time of compilation.  The newline
         is preserved for compatibility with earlier versions. */
      str1 = get_file_modification_time_string(file_name,
                                               /*strip_newline=*/FALSE);
      if (str1 == NULL) str1 = il_header.time_of_compilation;
      str2 = get_working_directory();
    } else {
      str1 = external_name;
      str2 = NULL;
    }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_flag_is_set("module_id")) {
      fprintf(f_debug, "make_module_id: str1 = %s, str2 = %s\n",
              str1, str2 == NULL ? "NULL" : str2);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    { /* The identifier is made of the primary source file name plus
         either the name of an externally defined variable or routine,
         or (if no such entity is available) the current directory and
         time and date.  If the entity name or time/date/directory is
         longer than 8 characters, a CRC of the string is used in place
         of the string.  Non-identifier characters are replaced with
         underscores. */
      size_t len1 = strlen(str1);
      size_t len2 = str2 == NULL ? 0 : strlen(str2);

      if ((len1 + len2 + (unsigned)(len2 != 0)) > 8) {
        /* The string (not including the file name) is longer than 8
           characters.  Use a CRC of the string instead. */
        unsigned long crc = crc_32(str1, (unsigned long)0);

        if (len2 != 0) {
          crc = crc_32(str2, crc);
        }  /* if */
        crc_buf.reset_to(left_pad(8, '0', hex_view_of(crc)));
        str1 = crc_buf.as_temp_characters();
        len1 = crc_buf.length();
        str2 = NULL;
        len2 = 0;
      }  /* if */
      /* Exclude the directory portion of the file name. */
      { a_const_char *end_of_dir;
        end_of_dir = end_of_directory_name(file_name);
        if (end_of_dir != NULL) file_name = end_of_dir+1;
      }
      file_name_len = strlen(file_name);

      /* The file name is preceded by its length enclosed in underscores. */
      a_number_buffer
                len_buf("_", file_name_len, "_");
      char      *mod_id = alloc_general(len_buf.length() + file_name_len + 1 +
                                        len1 + len2 + (unsigned)(len2 != 0) +
                                        1);
      (void)strcpy(mod_id, len_buf.as_temp_characters());
      (void)strcat(mod_id, file_name);
      (void)strcat(mod_id, "_");
      (void)strcat(mod_id, str1);
      if (str2 != NULL) {
        (void)strcat(mod_id, "_");
        (void)strcat(mod_id, str2);
      }  /* if */
      /* Change non-identifier characters to "_". */
      change_non_id_characters(mod_id);
      module_id = mod_id;
    }
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_flag_is_set("module_id")) {
      fprintf(f_debug, "make_module_id: final string = %s\n", module_id);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    /* If there are any functions whose lowering was delayed due to lack
       of a suitable module id, they can now be lowered (but can't be lowered
       here because lowering of another function may already be in
       progress). */
  }  /* if */
  return module_id;
}  /* make_module_id */

#endif /* MODULE_ID_NEEDED */

#if EDG_WIN32

static HANDLE CreateFile_interface(
				a_const_char		*file_name,
				DWORD			dwDesiredAccess,
				DWORD			dwShareMode,
				LPSECURITY_ATTRIBUTES	lpSecurityAttributes,
				DWORD			dwCreationDisposition,
				DWORD			dwFlagsAndAttributes,
				HANDLE			hTemplateFile)
/*
Interface to the Windows CreateFile routine.  If multibyte characters are
supported in the file name, handle that specially.  The parameters are
as specified by CreateFile except that we use our a_const_char* type for
file_name.
*/
{
  HANDLE	f_file;
#if UNICODE_SOURCE_SUPPORTED
  wchar_t *wide_file_name = translate_filename_to_wchar(file_name);

  if (wide_file_name != NULL) {
    /* The file name has embedded non-ASCII characters, so we need to use
       the CreateFileW routine. */
    f_file = CreateFileW(wide_file_name, dwDesiredAccess, dwShareMode,
                         lpSecurityAttributes,
                         dwCreationDisposition,
                         dwFlagsAndAttributes,
                         hTemplateFile);
  } else {
    /* The file name contained no special characters.  Just do a normal
       open. */
    f_file = CreateFileA(file_name, dwDesiredAccess, dwShareMode,
                         lpSecurityAttributes,
                         dwCreationDisposition,
                         dwFlagsAndAttributes,
                         hTemplateFile);
  }  /* if */
#else /* !UNICODE_SOURCE_SUPPORTED */
  /* Translate the file name into the form used by the file system. */
  file_name = file_name_in_external_encoding(file_name);
  f_file = CreateFileA(file_name, dwDesiredAccess, dwShareMode,
                       lpSecurityAttributes,
                       dwCreationDisposition,
                       dwFlagsAndAttributes,
                       hTemplateFile);
#endif /* UNICODE_SOURCE_SUPPORTED */
  return f_file;
}  /* CreateFile_interface */

#endif /* EDG_WIN32 */

#if UNIQUE_FILE_IDENTIFIER_AVAILABLE

void clear_unique_file_id(a_unique_file_id_ptr	ufip)
/*
Clear the fields in ufip, which is a structure used to uniquely identify
a file in a file system.  The cleared values must match the values that
are expected to represent a file for which unique identifier information
is not available in same_unique_file_ids.
*/
{
#if EDG_WIN32
  ufip->dwVolumeSerialNumber = 0;
  ufip->nFileIndexHigh = 0;
  ufip->nFileIndexLow = 0;
#else /* !EDG_WIN32 */
#if STAT_AVAILABLE
  ufip->st_dev = 0;
  ufip->st_ino = 0;
#else /* !STAT_AVAILABLE */
 #error An implementation of clear_unique_file_id must be supplied.
#endif /* STAT_AVAILABLE */
#endif /* EDG_WIN32 */
}  /* clear_unique_file_id */


void get_unique_id_for_file(a_const_char		*file_name,
			    a_unique_file_id_ptr	unique_id)
/*
Get the unique file identifier for file_name and store it in *unique_id.
If an error occurs attempting to get this information, *unique_id is
left unchanged.
*/
{
  a_const_char               *abs_file_name = resolve_file_name(file_name);
#if EDG_WIN32
  BY_HANDLE_FILE_INFORMATION file_info;
  HANDLE                     f_file;

  /* Make sure the unique ID has been initialized. */
  clear_unique_file_id(unique_id);
  /* Open the file so that we can get the file information. */
  f_file = CreateFile_interface(
                      abs_file_name, GENERIC_READ,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      (LPSECURITY_ATTRIBUTES)NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                      (HANDLE)NULL);
  /* If the file cannot be opened, or the call below to get the file
     information fails, leave the unique_id set to its default values. */
  if (f_file != INVALID_HANDLE_VALUE) {
    if (GetFileInformationByHandle(f_file, &file_info)) {
      unique_id->dwVolumeSerialNumber = file_info.dwVolumeSerialNumber;
      unique_id->nFileIndexLow = file_info.nFileIndexLow;
      unique_id->nFileIndexHigh = file_info.nFileIndexHigh;
    }  /* if */
    (void)CloseHandle(f_file);
  }  /* if */
#else /* !EDG_WIN32 */
#if STAT_AVAILABLE
  a_const_char *ext_file_name = file_name_in_external_encoding(abs_file_name);
  struct stat  buf;

  /* Make sure the unique ID has been initialized. */
  clear_unique_file_id(unique_id);
  if (stat(ext_file_name, &buf) == 0) {
    unique_id->st_dev = buf.st_dev;
    unique_id->st_ino = buf.st_ino;
  }  /* if */
#else /* !STAT_AVAILABLE */
 #error An implementation of get_unique_id_for_file must be supplied.
#endif /* STAT_AVAILABLE */
#endif /* EDG_WIN32 */
}  /* get_unique_id_for_file */


a_boolean same_unique_file_ids(a_unique_file_id_ptr	id1,
			       a_unique_file_id_ptr	id2)
/*
Compare the unique file identifiers pointed to by id1 and id2.   Return
TRUE if they are the same, or FALSE if they are different.  If either
of the identifiers have their default value (meaning a unique identifier
could not be determined), return FALSE.
*/
{
  a_boolean	result = FALSE;

#if EDG_WIN32
  result = id1->dwVolumeSerialNumber == id2->dwVolumeSerialNumber &&
           id1->nFileIndexLow == id2->nFileIndexLow &&
           id1->nFileIndexHigh == id2->nFileIndexHigh &&
           (id1->dwVolumeSerialNumber != 0 ||
            id1->nFileIndexLow != 0 ||
            id1->nFileIndexHigh != 0);
#else /* !EDG_WIN32 */
#if STAT_AVAILABLE
  result = id1->st_dev == id2->st_dev &&
           id1->st_ino == id2->st_ino &&
           (id1->st_dev != 0 ||
            id1->st_ino != 0);
#else /* !STAT_AVAILABLE */
 #error An implementation of same_unique_file_ids must be supplied.
#endif /* STAT_AVAILABLE */
#endif /* EDG_WIN32 */
  return result;
}  /* same_unique_file_ids */


a_hash_value hash_unique_file_id(a_unique_file_id_ptr	id)
/*
Produce a hash value for the unique file identifier "id".
*/
{
  a_hash_value	value = 0;

#if EDG_WIN32
  value = (a_hash_value)id->dwVolumeSerialNumber +
          (a_hash_value)id->nFileIndexLow +
          (a_hash_value)id->nFileIndexHigh;
#else /* !EDG_WIN32 */
#if STAT_AVAILABLE
  value = (a_hash_value)id->st_dev + (a_hash_value)id->st_ino;
#else /* !STAT_AVAILABLE */
 #error An implementation of hash_unique_file_id must be supplied.
#endif /* STAT_AVAILABLE */
#endif /* EDG_WIN32 */
  return value;
}  /* hash_unique_file_id */

#endif /* UNIQUE_FILE_IDENTIFIER_AVAILABLE */

#if USE_MMAP_FOR_MEMORY_REGIONS

#if EDG_WIN32

NORETURN static void str_GetLastError_catastrophe(an_error_code error_code,
                                                  a_const_char  *file_name)
/*
Use the WIN32 routines to get and format the last error that occurred.
Issue a catastrophic error using the specified error_code, file_name, and
the string returned that describes the error.
*/
{
  LPVOID lpMsgBuf;
  a_boolean msg_ok = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                     FORMAT_MESSAGE_FROM_SYSTEM |
                                     FORMAT_MESSAGE_IGNORE_INSERTS,
                                   /*lpSource=*/NULL,
                                   GetLastError(),
                                   /* Default language */
                                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                   (LPTSTR) &lpMsgBuf,
                                   /*nSize=*/0,
                                   /*Arguments=*/NULL);
  pos_str2_catastrophe(error_code, file_name,
                       msg_ok ? (char *)lpMsgBuf
                              : (char *)"(cannot determine reason)",
                       &error_position);
}  /* str_GetLastError_catastrophe */


an_mmap_handle open_memory_region_tmp_file()
/*
Open a temporary file to be used for allocation of file mapped
memory for IL memory blocks.
*/
{
  an_mmap_handle file;

  db_enter(3, "open_memory_region_tmp_file");
  /* Resolve the temporary directory if not already resolved (temp_dir). */
  resolve_temp_dir();

  a_temp_file_name_buffer file_name = get_win32_temp_file_name();
  if (file_name.is_empty()) {
    catastrophe(ec_cannot_build_temp_file_name);
  }  /* if */
  file = CreateFile_interface(
                           file_name.as_temp_characters(),
                           GENERIC_READ | GENERIC_WRITE,
                           /*fdwShareMode=*/0, (LPSECURITY_ATTRIBUTES)NULL,
                           CREATE_ALWAYS,
                           FILE_ATTRIBUTE_TEMPORARY |
                                                  FILE_FLAG_DELETE_ON_CLOSE,
                           (HANDLE)NULL);
  if (file == INVALID_HANDLE_VALUE) {
    str_GetLastError_catastrophe(ec_cannot_open_temp_file_reason,
                                 file_name.as_temp_characters());
  }  /* if */
  db_exit();
  return file;
}  /* open_memory_region_tmp_file */

#if MAKE_FRONT_END_CALLABLE

void close_memory_region_tmp_file(an_mmap_handle file)
/*
Close the file used for allocation of file mapped memory for IL memory blocks.
*/
{
  if (file != NULL) (void)CloseHandle(file);
}  /* close_memory_region_tmp_file */

#endif /* MAKE_FRONT_END_CALLABLE */

a_mapped_input_file open_mapped_input_file(a_const_char    *file_name,
                                           ARG_UNUSED FILE *open_file)
/*
Open a file that contains memory region information that will be mapped
into the address space of the current process.  This is used to reactivate
a precompiled header file or an open IFC module file.  This file will already
have been opened using fopen, so this open must be done in shared mode.
Returns an a_mapped_input_file holding the re-opened file handle and the
file mapping object handle.
*/
{
  a_mapped_input_file result;
  result.mapped_input = CreateFile_interface(
                              file_name, GENERIC_READ,
                              FILE_SHARE_READ, (LPSECURITY_ATTRIBUTES)NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_READONLY,
                              (HANDLE)NULL);
  check_assertion_str(result.mapped_input != INVALID_HANDLE_VALUE,
                      "CreateFile of mapped input file failed");
  if (result.mapped_input == INVALID_HANDLE_VALUE) {
    /* This shouldn't happen because the file must have already been
       successfully opened as a normal input file before this routine is
       called. */
    str_GetLastError_catastrophe(ec_cannot_open_pch_input_file_reason,
                                 file_name);
  }  /* if */
  result.map_object = CreateFileMapping(result.mapped_input, NULL,
                                        PAGE_WRITECOPY, 0, 0, NULL);
  check_assertion_str(result.map_object != INVALID_HANDLE_VALUE,
                      "CreateFileMapping failed");
  if (result.map_object == INVALID_HANDLE_VALUE) {
    str_GetLastError_catastrophe(ec_unable_to_get_mapped_memory_reason,
                                 file_name);
  }  /* if */
  return result;
}  /* open_mapped_input_file */


void close_mapped_input_file(a_mapped_input_file file)
/*
Close the mapped input file and the associated map object.
*/
{
  if (!CloseHandle(file.mapped_input)) {
    unexpected_condition_str("CloseHandle of mapped input failed");
  }  /* if */
  if (!CloseHandle(file.map_object)) {
    unexpected_condition_str("CloseHandle of map object failed");
  }  /* if */
}  /* close_mapped_input_file */


a_void_ptr map_memory_region_file(an_mmap_handle handle,
                                  void           *base_addr,
                                  sizeof_t       curr_size,
                                  sizeof_t       incremental_size,
                                  sizeof_t       file_offset)
/*
Expand the memory region file represented by the given handle.  This routine
assumes that curr_size bytes have already been allocated and mapped starting at
base_addr, and that incremental_size bytes should be added.  If base_addr is
NULL, the memory region mapped will be picked by the system; otherwise, the
memory mapped region starts at base_addr.  incremental_size must be a multiple
of the host allocation granularity.
*/
{
  a_void_ptr	addr = NULL;
  LARGE_INTEGER	file_pos;
  LARGE_INTEGER	large_file_offset;
  DWORD		bytes_written;
  HANDLE	f_map;
#if DEBUG
/* GCOVR_EXCL_START */
  const char*	failed_system_call = NULL;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  a_void_ptr    map_address = (base_addr != NULL) ?
              (a_void_ptr)((char*)base_addr + curr_size) : NULL;

  db_enter(4, "map_memory_region_file");
  file_pos.QuadPart = file_offset + incremental_size;
  /* The file must be large enough to contain the mapped area. */
  if (SetFilePointerEx(handle, file_pos,
                       (PLARGE_INTEGER)NULL, FILE_BEGIN)) {
    /* Write a character at the last allocated position. */
    if (WriteFile(handle, &file_pos, 1, &bytes_written,
                  (LPOVERLAPPED)NULL)) {
      f_map = CreateFileMapping(handle,
                                (LPSECURITY_ATTRIBUTES)NULL,
                                PAGE_READWRITE, (DWORD)0, (DWORD)0,
                                (LPTSTR)NULL);
      if (f_map != INVALID_HANDLE_VALUE) {
        large_file_offset.QuadPart = file_offset;
        if (map_address != NULL) {
          addr = MapViewOfFileEx(f_map, FILE_MAP_WRITE,
                                 large_file_offset.HighPart,
                                 large_file_offset.LowPart, incremental_size,
                                 map_address);
#if DEBUG
/* GCOVR_EXCL_START */
          if (addr == NULL) {
            failed_system_call = "MapViewOfFileEx";
          }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        } else {
          addr = MapViewOfFile(f_map, FILE_MAP_WRITE,
                               large_file_offset.HighPart,
                               large_file_offset.LowPart,
                               incremental_size);
#if DEBUG
/* GCOVR_EXCL_START */
          if (addr == NULL) {
            failed_system_call = "MapViewOfFile";
          }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        }  /* if */
        /* The handle for mapping is not needed after the view is obtained
           above. */
        if (!CloseHandle(f_map)) {
#if DEBUG
/* GCOVR_EXCL_START */
          if (failed_system_call == NULL) {
            failed_system_call = "CloseHandle";
          }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
      } else {
        failed_system_call = "CreateFileMapping";
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
      if (db_flag_is_set("mmap") || debug_level >= 4) {
        fprintf(f_debug,
                "map_memory_region_file: allocated %lu bytes of mmap memory "
                "at %p\n",
                (unsigned long)incremental_size, addr);
        if (map_address != NULL) {
          fprintf(f_debug, "  requested address was: %p\n", map_address);
        }  /* if */
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#if DEBUG
/* GCOVR_EXCL_START */
    } else {
      failed_system_call = "WriteFile";
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  } else {
    failed_system_call = "SetFilePointerEx";
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (failed_system_call != NULL && db_flag_is_set("mmap")) {
    DWORD err_code = GetLastError();
    if (map_address != NULL) {
      fprintf(f_error, "map_memory_region_file(0x%Ix, 0x%Ix, 0x%Ix) at %p:",
              curr_size, incremental_size, file_offset, map_address);
    } else {
      fprintf(f_error, "map_memory_region_file(0x%Ix, 0x%Ix, 0x%Ix):",
              curr_size, incremental_size, file_offset);
    }  /* if */
    fprintf(f_error, " %s() failed with error code %d\n",
            failed_system_call, err_code);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  db_exit();
  return addr;
}  /* map_memory_region_file */


a_void_ptr map_input_file_to_region(a_mapped_input_file file,
                                    a_boolean           read_only,
                                    sizeof_t            offset,
                                    sizeof_t            size,
                                    a_void_ptr          address,
                                    a_const_char        *file_name)
/*
The data pointed to by "file" is mapped, starting at "offset" bytes, for
"size" bytes to the address specified by "address".  This mapping is done as
either a FILE_MAP_COPY mapping so that any changes to the data will be local
(when read_only is FALSE), or FILE_MAP_READ for read-only access (when
read_only is TRUE).  This is used to map a section of a PCH file or module file
to a memory region.  If the memory cannot be mapped, a catastrophic error is
issued.  file_name is the name of the mapped input file to be used if a
diagnostic is issued.  Returns the mapped address (or issues a catastrophic
error).
*/
{
  a_void_ptr    result_addr;
  LARGE_INTEGER large_file_offset;

  large_file_offset.QuadPart = offset;
  result_addr = MapViewOfFileEx(file.map_object,
                                read_only ? FILE_MAP_READ : FILE_MAP_COPY,
                                large_file_offset.HighPart,
                                large_file_offset.LowPart, size, address);
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("mmap") || debug_level >= 4) {
    fprintf(f_debug,
        "map_input_file_to_region: allocated %lu bytes of mmap memory at %p\n",
            (unsigned long)size, address);
  }  /* if */
  if ((db_flag_is_set("mmap") || debug_level >= 1) && result_addr == NULL) {
    fprintf(f_debug, "Map failed: address=%p, size=%lu, offset=%lu\n",
            address, (unsigned long)size, (unsigned long)offset);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  if (result_addr == NULL) {
    error_position = null_source_position;
    str_GetLastError_catastrophe(ec_unable_to_get_mapped_memory_reason,
                                 file_name);
  }  /* if */
  return result_addr;
}  /* map_input_file_to_region */


void unmap_memory(a_void_ptr          addr,
                  ARG_UNUSED sizeof_t size)
/*
Unmap a block of previously mapped memory.
*/
{
  if (!UnmapViewOfFile(addr)) {
    unexpected_condition_str("unmap_memory: UnmapViewOfFile failed\n");
  }  /* if */
}  /* unmap_memory */


size_t get_page_size()
/*
Return the size of the host allocation granularity.

When map_memory_region_file is called, incremental_size must be a multiple of
the allocation granularity for MapViewOfFileEx.  As the allocation granularity
is a multiple of page size the front end (in the interest of a simpler host
interface and historic reasons) uses the allocation granularity for the page
size on Windows in place of an additional allocation granularity function.
*/
{
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwAllocationGranularity;
}  /* get_page_size */


#else /* !EDG_WIN32 */
#include <sys/mman.h>

#if defined(__hpux) || defined(__AIX__)
/* HP and IBM require that unistd.h be included, and use _SC_PAGE_SIZE
   in place of _SC_PAGESIZE. */ 
#include <unistd.h>
#define _SC_PAGESIZE _SC_PAGE_SIZE
#endif /* defined(__hpux) || defined(__AIX__) */

#if defined(__linux__) && defined(MAP_FIXED_NOREPLACE)
#define MMAP_FIXED_NOREPLACE_FLAG 1
#else /* !(defined(__linux__) && defined(MAP_FIXED_NOREPLACE)) */
#define MMAP_FIXED_NOREPLACE_FLAG 0
#endif /* defined(__linux__) && defined(MAP_FIXED_NOREPLACE) */

#if MMAP_FIXED_NOREPLACE_FLAG
STATIC_THREAD a_boolean
		try_mmap_fixed_noreplace;
			/* TRUE when MAP_FIXED_NOREPLACE should be attempted.
			   Set to FALSE the first time the kernel ignores the
			   flag and returns a system-assigned address, so that
			   subsequent calls skip directly to MAP_FIXED. */
#endif /* MMAP_FIXED_NOREPLACE_FLAG */

size_t get_page_size()
/*
Return the size of a host page.  When map_memory_region_file is called,
incremental_size must be a multiple of the page size.
*/
{
  long page_size = sysconf(_SC_PAGESIZE);
  check_assertion_str2(page_size > 0, "get_page_size:", "invalid page size");
  return (size_t)page_size;
}  /* get_page_size */


static inline caddr_t mmap_posix(a_void_ptr address,
                                 sizeof_t   size,
                                 int        prot,
                                 int        fd,
                                 sizeof_t   offset)
/*
Perform a POSIX mmap of the data pointed to by the given file descriptor "fd".
The data is mapped starting at "offset" bytes for "size" bytes, to the address
specified by "address" (or a system-chosen location if "address" is NULL).
The mapping never affects the opened file (due to the use of MAP_PRIVATE).

If "address" is non-NULL, this function is guaranteed to return an address
equal to the given "address" or NULL.  Otherwise, the return value is the
system chosen memory address or NULL if the system could not find a suitable
address.

Note: On Linux when "address" is non-NULL, this function uses the
Linux-specific mmap flag MAP_FIXED_NOREPLACE on Linux Kernel 4.17 and newer;
this eliminates the risk of mmap clobbering other memory allocations.  At the
time of writing, other POSIX mmap implementations do not provide this
functionality and thus this call may clobber memory if the address is not
carefully chosen.
*/
{
  caddr_t addr = NULL;

  if (address != NULL) {
#if MMAP_FIXED_NOREPLACE_FLAG
    if (try_mmap_fixed_noreplace) {
      addr = (caddr_t)mmap((caddr_t)address, size, prot,
                           MAP_PRIVATE | MAP_FIXED_NOREPLACE,
                           fd, (off_t)offset);
      /* On older Linux kernels MAP_FIXED_NOREPLACE is silently ignored and
         mmap may return a system-assigned address rather than the one
         requested.  Detect this case, disable MAP_FIXED_NOREPLACE for all
         future calls, unmap the misplaced region, and retry with MAP_FIXED. */
      if (addr == (caddr_t)address || addr == (caddr_t)-1) {
        /* The MAP_FIXED_NOREPLACE mmap flag was supported: keep this result
           and do not fall back to MAP_FIXED. */
      } else {
        /* The MAP_FIXED_NOREPLACE mmap flag was not supported: prevent further
           attempts at using MAP_FIXED_NOREPLACE and retry. */
        try_mmap_fixed_noreplace = FALSE;
        (void)munmap(addr, size);
        addr = (caddr_t)mmap((caddr_t)address, size, prot,
                             MAP_PRIVATE | MAP_FIXED, fd, (off_t)offset);
      }  /* if */
    } else
#endif /* MMAP_FIXED_NOREPLACE_FLAG */
    /* Do not add code here. */
    {
      addr = (caddr_t)mmap((caddr_t)address, size, prot,
                           MAP_PRIVATE | MAP_FIXED, fd, (off_t)offset);
    }  /* if */
  } else {
    addr = (caddr_t)mmap((char*)0, size, prot, MAP_PRIVATE, fd, (off_t)offset);
  }  /* if */
  if (addr == (caddr_t)-1) addr = NULL;
  return addr;
}  /* mmap_posix */


a_void_ptr map_memory_region_file(an_mmap_handle handle,
                                  void           *base_addr,
                                  sizeof_t       curr_size,
                                  sizeof_t       incremental_size,
                                  sizeof_t       file_offset)
/*
Expand the memory region file represented by the given handle.  This routine
assumes that curr_size bytes have already been allocated and mapped starting at
base_addr, and that incremental_size bytes should be added.  If base_addr is
NULL, the memory region mapped will be picked by the system; otherwise, the
memory mapped region starts at base_addr.  incremental_size must be a multiple
of the host page size.
*/
{
  caddr_t    addr = NULL;
  sizeof_t   size;
  a_void_ptr map_address = (base_addr != NULL) ?
           (a_void_ptr)((char*)base_addr + curr_size) : NULL;

  db_enter(4, "map_memory_region_file");
  size = file_offset + incremental_size;
  /* The file must be large enough to contain the mapped area. */
  if (fseek(handle, (long)size, SEEK_SET) == 0) {
    /* Write a character at the last allocated position and
       make sure the write to the file is actually done. */
    if (fputc(0, handle) != EOF && fflush(handle) == 0) {
      addr = mmap_posix(map_address, incremental_size, PROT_WRITE | PROT_READ,
                        fileno(handle), file_offset);
#if DEBUG
/* GCOVR_EXCL_START */
      if (db_flag_is_set("mmap") || debug_level >= 4) {
        fprintf(f_debug,
                "map_memory_region_file: allocated %lu bytes of mmap memory "
                "at %p\n",
                (unsigned long)incremental_size, (a_void_ptr)addr);
        if (map_address != NULL) {
          fprintf(f_debug, "  requested address was: %p\n", map_address);
        }  /* if */
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
  db_exit();
  return addr;
}  /* map_memory_region_file */


a_mapped_input_file open_mapped_input_file(ARG_UNUSED a_const_char *file_name,
                                           FILE                    *open_file)
/*
Return an a_mapped_input_file wrapping the already-open file.  file_name is
unused on POSIX because the file has already been opened by the caller.
*/
{
  a_mapped_input_file result;
  result.file = open_file;
  return result;
}  /* open_mapped_input_file */


void close_mapped_input_file(ARG_UNUSED a_mapped_input_file file)
/*
On POSIX the file is owned by the caller; this function is a no-op.
*/
{
}  /* close_mapped_input_file */


a_void_ptr map_input_file_to_region(a_mapped_input_file file,
                                    a_boolean           read_only,
                                    sizeof_t            offset,
                                    sizeof_t            size,
                                    a_void_ptr          address,
                                    a_const_char        *file_name)
/*
The data pointed to by "file" is mapped, starting at "offset" bytes for "size"
bytes, to the address specified by "address".  The mapping never affects the
opened file (due to the use of MAP_PRIVATE).  However, if read_only is set the
buffer cannot be manipulated; otherwise, the buffer can be freely modified
without affecting the file.  If the memory cannot be mapped, a catastrophic
error is issued.  file_name is the name of the mapped input file to be used if
a diagnostic is issued.  Returns the mapped address (or issues a catastrophic
error).
*/
{
  int		fd = fileno(file.file); /*lint !e718 !e746*/
  a_void_ptr	result_addr;

  result_addr = (a_void_ptr)mmap_posix(address, size,
                                       read_only ? PROT_READ :
                                                   PROT_READ | PROT_WRITE,
                                       fd, offset);
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("mmap") || debug_level >= 4) {
    fprintf(f_debug,
        "map_input_file_to_region: allocated %lu bytes of mmap memory at %p\n",
            (unsigned long)size, address);
  }  /* if */
  if ((db_flag_is_set("mmap") || debug_level >= 1) && result_addr == NULL) {
    fprintf(f_debug, "Map failed: address=%p, size=%lu, offset=%lu\n",
            address, (unsigned long)size, (unsigned long)offset);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  if (result_addr == NULL) {
    error_position = null_source_position;
    str_errno_catastrophe(ec_unable_to_get_mapped_memory_reason, file_name,
                          errno);
  }  /* if */
  return result_addr;
}  /* map_input_file_to_region */


void unmap_memory(a_void_ptr	addr,
	          sizeof_t	size)
/*
Unmap a block of previously mapped memory.
*/
{
  if (munmap((caddr_t)addr, size) != 0) {
    unexpected_condition_str("unmap_memory: munmap failed\n");
  }  /* if */
}  /* unmap_memory */


an_mmap_handle open_memory_region_tmp_file()
/*
Open a temporary file to be used for allocation of file mapped
memory for IL memory blocks.
*/
{
  an_mmap_handle file;

  db_enter(3, "open_memory_region_tmp_file");
  file = open_temp_file(/*binary_file=*/TRUE);
  /*lint -e{530}*/ /* Lint bug PCLP-804 */
  check_assertion(file != NULL);
  db_exit();
  return file;
}  /* open_memory_region_tmp_file */

#if MAKE_FRONT_END_CALLABLE

void close_memory_region_tmp_file(an_mmap_handle file)
/*
Close the file used for allocation of file mapped memory for IL memory blocks.
*/
{
  if (file != NULL) (void)fclose(file);
}  /* close_memory_region_tmp_file */

#endif /* MAKE_FRONT_END_CALLABLE */

#endif /* EDG_WIN32 */

STATIC_THREAD size_t
		page_size;
			/* The size of a host page.  Memory mapped blocks must
			   be requested in increments of this size. */


sizeof_t do_page_alignment(sizeof_t size)
/*
Return "size" adjusted as needed to be a multiple of the system page size.
*/
{
  sizeof_t	size2;

  /* On the first call of this routine, get the host page size. */
  if (page_size == 0) {
    page_size = get_page_size();
    /* The host allocation increment must be a multiple of the page size. */
    check_assertion_str(HOST_ALLOCATION_INCREMENT % page_size == 0,
                        "invalid HOST_ALLOCATION_INCREMENT for page size");
  }  /* if */
  size2 = (size / (sizeof_t)(unsigned int)page_size) * (sizeof_t)page_size;
  if (size2 < size) {
    size2 += (sizeof_t)page_size;
  }  /* if */
  return size2;
}  /* do_page_alignment */


sizeof_t seek_to_page_alignment(FILE *file)
/*
Seeks to the next position in the file that is a multiple of the
host page size.  This is used to ensure that data written to a file
is at an offset that can be used as an argument to mmap.  Return the
current file position.
*/
{
  sizeof_t	curr_pos;

#if EDG_WIN32
  /* A special version is needed for Windows because on 64-bit versions
     of Windows "long" is 32 bits. */
  curr_pos = (size_t)_ftelli64(file);
  curr_pos = do_page_alignment(curr_pos);
  if (_fseeki64(file, (size_t)curr_pos, SEEK_SET) != 0) {
    unexpected_condition_str("seek_to_page_alignment: fseek error");
  }  /* if */
#else /* !EDG_WIN32 */
  curr_pos = (sizeof_t)ftell(file);
  curr_pos = do_page_alignment(curr_pos);
  if (fseek(file, (long)curr_pos, SEEK_SET) != 0) {
    unexpected_condition_str("seek_to_page_alignment: fseek error");
  }  /* if */
#endif /* EDG_WIN32 */
  return curr_pos;
}  /* seek_to_page_alignment */

#endif /* USE_MMAP_FOR_MEMORY_REGIONS */

#if SVR4_TRAP_NULL_POINTER_REFERENCES
#if !USE_MMAP_FOR_MEMORY_REGIONS
/* This will already have been included if we are using mmap. */
#include "mman.h"
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */

static void svr4_trap_null_pointer_references(void)
/*
Some systems don't trap NULL pointer references.  This routine may
be used on SVR4 systems to have NULL pointer references result in
a memory fault.
*/
{
  int	*p;
  p = 0;
  /* Access page zero.  This needs to be done before the mprotect call. */
  p = (int*)(*p);
  /* Protect the first 4k bytes of memory. */
  if (mprotect(0, 4096, PROT_NONE) == -1) {
    unexpected_condition_str("mprotect failed");
  }  /* if */
}  /* svr4_trap_null_pointer_references */
#endif /* SVR4_TRAP_NULL_POINTER_REFERENCES */

#if !EDG_WIN32 && !MULTIPLE_THREAD_COMPILATION

#if __BSD__
#include <sys/time.h>
#endif /* __BSD__ */
#include <sys/resource.h>

void set_cpu_time_limit(int	seconds)
/*
Set the maximum amount of CPU time that can be used by the compilation.
Used for debugging purposes.
*/
{
  struct rlimit	limit;
  (void)getrlimit((int)RLIMIT_CPU, &limit);
  limit.rlim_cur = (rlim_t)seconds;
  (void)setrlimit((int)RLIMIT_CPU, &limit);
}  /* set_cpu_time_limit */


static void reset_cpu_time_limit(void)
/*
Reset the maximum amount of CPU time that can be used by the compilation
in case it had been previously changed by set_cpu_time_limit.
*/
{
  struct rlimit	limit;
  (void)getrlimit((int)RLIMIT_CPU, &limit);
  limit.rlim_cur = RLIM_INFINITY;
  (void)setrlimit((int)RLIMIT_CPU, &limit);
}  /* reset_cpu_time_limit */

#endif /* !EDG_WIN32 && !MULTIPLE_THREAD_COMPILATION */

#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED

int f_mbc_length(a_const_char		*ptr,
                 a_boolean		*err,
                 ARG_UNUSED a_boolean	is_native)
/*
Return the length of the multibyte character sequence beginning at ptr.
If the sequence there is invalid, set *err to TRUE if err is non-NULL,
and return a length appropriate for error recovery.  This function should
usually be called via the macro mbc_length.  Note that, unlike the standard
mblen, this routine does not return 0 when given a null (zero) character;
it returns 1.

When Unicode source is supported, is_native indicates whether the multibyte
encoding is Unicode (non-native) or some other encoding (native).
When NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE is TRUE, this routine
must be able to determine the length of a multibyte character sequence in
some other (non-Unicode) encoding.  The EDG-supplied version of this routine
only supports this capability when EDG_WIN32 is TRUE (because it relies on
Windows routines), in which case the locale to be used for the multibyte
encoding is specified by native_multibyte_locale.

When NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE is FALSE, native characters
are assumed to be Latin-1.
*/
{
  int len = 0;

  if (err != NULL) *err = FALSE;
#if USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING
  /* Use custom code for SJIS instead of the C library routines. */
  { unsigned char ch = (unsigned char)*ptr;
    /* Check for codes that indicate the first character of a two-character
       sequence.  Note that the test functions are macros that can be
       replaced as necessary to deal with the fact that different
       implementations of SJIS use different character ranges. */
    if (is_first_char_of_sjis_two_char_sequence(ch)) {
      /* Two-character sequence.  Check validity of second character. */
      ch = (unsigned char)(ptr[1]);
      if (is_valid_sjis_second_char(ch)) {
        len = 2;
      } else {
        /* Invalid sequence.  Advance bytewise. */
        if (err != NULL) *err = TRUE;
        len = 1;
      }  /* if */
    } else {
      /* One-character sequence. */
      len = 1;
    }  /* if */
  }
#else /* !USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING */
#if EDG_MULTIBYTE_CHAR_TEST_MODE
  /* In EDG multibyte test mode, a "$" is treated as the start of a multibyte
     character unless it is the last character of the string. */
  len = *ptr == '$' && *(ptr+1) != '\0' ? 2 : 1;
#else /* !EDG_MULTIBYTE_CHAR_TEST_MODE */
#if UNICODE_SOURCE_SUPPORTED
  if (!is_native) {
    /* UTF-8. */
    unsigned char ch = (unsigned char)*ptr;
    if (ch <= 0x7f) {
      /* Simple one-byte character. */
      len = 1;
    } else {
      a_boolean local_err = FALSE;
      if ((ch & 0xe0) == 0xc0) {
        /* Top three bits are 110: start of a two-byte sequence.  Second byte
           must have 10 as top two bits. */
        if (((unsigned char)ptr[1] & 0xc0) == 0x80) {
          len = 2;
        } else {
          local_err = TRUE;
        }  /* if */
      } else if ((ch & 0xf0) == 0xe0) {
        /* Top four bits are 1110: start of a three-byte sequence.  Second and
           third bytes must have 10 as top two bits. */
        if (((unsigned char)ptr[1] & 0xc0) == 0x80 &&
            ((unsigned char)ptr[2] & 0xc0) == 0x80) {
          len = 3;
        } else {
          local_err = TRUE;
        }  /* if */
      } else if ((ch & 0xf8) == 0xf0) {
        /* Top five bits are 11110: start of a four-byte sequence.  Second,
           third, and fourth bytes must have 10 as top two bits. */
        if (((unsigned char)ptr[1] & 0xc0) == 0x80 &&
            ((unsigned char)ptr[2] & 0xc0) == 0x80 &&
            ((unsigned char)ptr[3] & 0xc0) == 0x80) {
          len = 4;
        } else {
          local_err = TRUE;
        }  /* if */
      } else {
        /* First byte is invalid (e.g., it's a continuation byte having 10
           in the top two bits). */
        local_err = TRUE;
      }  /* if */
      if (local_err) {
        if (err != NULL) *err = TRUE;
        len = 1;
        /* Keep advancing to a character that's not a continuation. */
        while (((unsigned char)ptr[len] & 0xc0) == 0x80) len++;
      }  /* if */
    }  /* if */
  } else {
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
#if EDG_WIN32
    /* Note that this code is Windows-specific and must be customized for
       other platforms. */
    len = _mblen_l(ptr, MB_LEN_MAX, native_multibyte_locale);
#else /* !EDG_WIN32 */
#if EDG_NATIVE_MULTIBYTE_TEST_MODE
    /* Use standard C library routines. */
    len = mblen(ptr, MB_CUR_MAX);
#else /* !EDG_NATIVE_MULTIBYTE_TEST_MODE */
    #error f_mbc_length requires customization on non-Windows platforms when \
           using NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE.
#endif /* EDG_NATIVE_MULTIBYTE_TEST_MODE */
#endif /* EDG_WIN32 */
    if (len <= 0) {
      if (len == 0 && *ptr == '\0') {
        /* mblen returns 0 for a null character, but we want 1 for that. */
        len = 1;
      } else {
        /* Invalid multibyte sequence.  Advance bytewise. */
        if (err != NULL) *err = TRUE;
        len = 1;
      }  /* if */
    }  /* if */
#else /* !NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
    /* A native character is assumed to be in Latin-1. */
    len = 1;
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
  }  /* if */
#else /* !UNICODE_SOURCE_SUPPORTED */
  /* Use standard C library routines. */
  len = mblen(ptr, MB_CUR_MAX);
  if (len <= 0) {
    if (len == 0 && *ptr == '\0') {
      /* mblen returns 0 for a null character, but we want 1 for that. */
      len = 1;
    } else {
      /* Invalid multibyte sequence.  Advance bytewise. */
      if (err != NULL) *err = TRUE;
      len = 1;
    }  /* if */
  }  /* if */
#endif /* UNICODE_SOURCE_SUPPORTED */
#endif /* EDG_MULTIBYTE_CHAR_TEST_MODE */
#endif /* USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING */

  return len;
}  /* f_mbc_length */


int mbc_to_wide_char(a_const_char         *mb,
                     unsigned long        *wc,
                     a_boolean            *err,
                     ARG_UNUSED a_boolean is_native)
/*
Convert a multibyte character sequence pointed to by mb to a single wide
character returned in *wc.  Return the number of characters in the
multibyte character sequence.  If the multibyte character sequence is
invalid, set *err to TRUE if err is non-NULL, and return a length appropriate
for error recovery.

When Unicode source is supported, is_native indicates whether the multibyte
encoding is Unicode (non-native) or some other encoding (native).  The
character returned in *wc is always Unicode.  This means that when
NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE is TRUE, native characters must
be translated into Unicode.  The EDG-supplied version of this routine
only supports this translation when EDG_WIN32 is TRUE (because it relies
on Windows routines to do the translation), in which case the locale to be
used for the multibyte translation is specified by native_multibyte_locale.

When NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE is FALSE, native characters
are assumed to be Latin-1.
*/
{
  int       numch = 0;
  a_boolean local_err = FALSE;

#if USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING || \
    EDG_MULTIBYTE_CHAR_TEST_MODE
  /* Use custom code for SJIS instead of the C library routines.  This
     is also used in EDG multibyte test mode. */
  numch = mbc_length(mb, &local_err);
  if (local_err) {
    /* Bad multibyte character. */
    numch = 1;
    *wc = 0;
  } else {
    *wc = (unsigned char)mb[0];
    if (numch != 1) {
      *wc = (*wc << targ_char_bit) | (unsigned char)mb[1];
      check_assertion(numch == 2);
    }  /* if */
  }  /* if */
#else /* !(USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING ||
           EDG_MULTIBYTE_CHAR_TEST_MODE) */
#if UNICODE_SOURCE_SUPPORTED
  if (!is_native) {
    /* UTF-8. */
    unsigned char ch = (unsigned char)*mb;
    if (ch <= 0x7f) {
      /* Simple one-byte character. */
      numch = 1;
      *wc = ch;
    } else {
      if ((ch & 0xe0u) == 0xc0u) {
        /* Top three bits are 110: start of a two-byte sequence.  Second byte
           must have 10 as top two bits. */
        if (((unsigned char)mb[1] & 0xc0u) == 0x80u) {
          numch = 2;
          *wc = (ch & 0x1fu) << 6 |
                ((unsigned char)mb[1] & 0x3fu);
        } else {
          local_err = TRUE;
        }  /* if */
      } else if ((ch & 0xf0u) == 0xe0u) {
        /* Top four bits are 1110: start of a three-byte sequence.  Second and
           third bytes must have 10 as top two bits. */
        if (((unsigned char)mb[1] & 0xc0u) == 0x80u &&
            ((unsigned char)mb[2] & 0xc0u) == 0x80u) {
          numch = 3;
          *wc = (ch & 0xfu) << 12 |
                ((unsigned char)mb[1] & 0x3fu) << 6 |
                ((unsigned char)mb[2] & 0x3fu);
        } else {
          local_err = TRUE;
        }  /* if */
      } else if ((ch & 0xf8) == 0xf0) {
        /* Top five bits are 11110: start of a four-byte sequence.  Second,
           third, and fourth bytes must have 10 as top two bits. */
        if (((unsigned char)mb[1] & 0xc0u) == 0x80u &&
            ((unsigned char)mb[2] & 0xc0u) == 0x80u &&
            ((unsigned char)mb[3] & 0xc0u) == 0x80u) {
          numch = 4;
          *wc = (ch & 0x7u) << 18 |
                ((unsigned char)mb[1] & 0x3fu) << 12 |
                ((unsigned char)mb[2] & 0x3fu) << 6 |
                ((unsigned char)mb[3] & 0x3fu);
        } else {
          local_err = TRUE;
        }  /* if */
      } else {
        /* First byte is invalid (e.g., it's a continuation byte having 10
           in the top two bits). */
        local_err = TRUE;
      }  /* if */
      if (local_err) {
        *wc = 0;
        numch = 1;
        /* Keep advancing to a character that's not a continuation. */
        while (((unsigned char)mb[numch] & 0xc0) == 0x80) numch++;
      }  /* if */
    }  /* if */
  } else {
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
    wchar_t wchar;
#if EDG_WIN32
    /* Note that this code is Windows-specific and must be customized for
       other platforms.  _mbtowc_l converts a multibyte character sequence
       in the locale specified by native_multibyte_locale to a Unicode
       value. */
    numch = _mbtowc_l(&wchar, mb, MB_LEN_MAX, native_multibyte_locale);
#else /* !EDG_WIN32 */
#if EDG_NATIVE_MULTIBYTE_TEST_MODE
    /* Use standard C library routines.  Note that this does not do
       conversion to Unicode. */
    numch = mbtowc(&wchar, mb, MB_CUR_MAX);
#else /* !EDG_NATIVE_MULTIBYTE_TEST_MODE */
    #error mbc_to_wide_char requires customization on non-Windows platforms \
           when using NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE.
#endif /* EDG_NATIVE_MULTIBYTE_TEST_MODE */
#endif /* EDG_WIN32 */
    if (numch < 0) {
      /* Invalid multibyte character sequence. */
      numch = 1;
      *wc = 0;
      local_err = TRUE;
    } else {
      *wc = wchar;
    }  /* if */
#else /* !NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
    /* A native character is assumed to be in Latin-1. */
    *wc = (unsigned char)*mb;
    numch = 1;
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
  }  /* if */
#else /* !UNICODE_SOURCE_SUPPORTED */
  /* Use a standard C library routine to do the multibyte character
     sequence to wide character conversion. */
  { wchar_t wchar;
    numch = mbtowc(&wchar, mb, MB_CUR_MAX);
    if (numch < 0) {
      /* Invalid multibyte character sequence. */
      numch = 1;
      *wc = 0;
      local_err = TRUE;
    } else {
      *wc = wchar;
    }  /* if */
  }
#endif /* UNICODE_SOURCE_SUPPORTED */
#endif /* USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING ||
          EDG_MULTIBYTE_CHAR_TEST_MODE */
  if (err != NULL) *err = local_err;
  return numch;
}  /* mbc_to_wide_char */

#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
#if UNICODE_SOURCE_SUPPORTED

void clear_getc_source_state(a_getc_source_state   *state,
                             a_unicode_source_kind ukind)
/*
Clear a state block used by getc_source.  ukind indicates the kind of
Unicode characters in the file under scan, or usk_none if the file is
not Unicode.
*/
{
  state->count = 0;
  state->unicode_source_kind = ukind;
}  /* clear_getc_source_state */


int getc_utf16(FILE                *file,
               a_getc_source_state *state)
/*
Called from getc_source to fetch one character of UTF-16 input.  The
stream of characters returned on successive calls is the UTF-8 version
of the UTF-16 source.  file indicates the source file.  state is a pushback
queue used to save characters so that they can be returned on subsequent
calls of this routine.
*/
{
  int ch;

  if (state->count != 0) {
    /* Return a character from the pushback queue. */
    ch = state->chars[--state->count];
  } else {
    /* Read another UTF-16 character from the source file. */
    int ch1, ch2;
    unsigned long uc, uc2;
    ch1 = getc(file);
    if (ch1 == EOF) {
      /* End of file encountered. */
      ch = EOF;
      goto have_ch;
    }  /* if */
    /* Get the second byte of the UTF-16 character. */
    ch2 = getc(file);
    if (ch2 == EOF) {
      /* End of file encountered (weirdly, input doesn't have an even number
         of bytes). */
      ch = EOF;
      goto have_ch;
    }  /* if */
    /* Assemble two bytes into one 16-bit character. */
    ch1 = (unsigned char)ch1;
    ch2 = (unsigned char)ch2;
    if (state->unicode_source_kind == usk_utf16LE) {
      /* Little-endian assembly. */
      uc = (unsigned long)((ch2 << 8) | ch1);
    } else {
      /* Big-endian assembly. */
      uc = (unsigned long)((ch1 << 8) | ch2);
    }  /* if */
    if (uc < 0xd800 || uc >= 0xe000) {
      /* This is a single two-byte UTF-16 character. */
    } else if (uc >= 0xd800 && uc <= 0xdbff) {
      /* This is a first surrogate of a pair.  Fetch the second surrogate. */
      ch1 = getc(file);
      if (ch1 == EOF) {
        /* End of file, second surrogate is missing. */
        ch = EOF;
        goto have_ch;
      }  /* if */
      ch2 = getc(file);
      if (ch2 == EOF) {
        /* End of file, second byte of second surrogate is missing. */
        ch = EOF;
        goto have_ch;
      }  /* if */
      /* Assemble two bytes into one 16-bit character for the
         second surrogate. */
      ch1 = (unsigned char)ch1;
      ch2 = (unsigned char)ch2;
      if (state->unicode_source_kind == usk_utf16LE) {
        /* Little-endian assembly. */
        uc2 = (unsigned long)((ch2 << 8) | ch1);
      } else {
        /* Big-endian assembly. */
        uc2 = (unsigned long)((ch1 << 8) | ch2);
      }  /* if */
      if (uc2 < 0xdc00 || uc2 > 0xdfff) {
        /* Bad second surrogate. */
        uc = '?';
      } else {
        /* Combine the first and second surrogates into a single Unicode
           character. */
        uc = (((uc & 0x3ff) << 10) | (uc2 & 0x3ff)) + 0x10000;
      }  /* if */
    } else {
      /* Invalid first surrogate (e.g., a second surrogate appears first). */
      uc = '?';
    }  /* if */
    /* Now, uc is a single Unicode code point.  Encode it in one or more
       characters as UTF-8.   The first byte indicates the number of bytes
       (N bytes ==> for N==1, top bit is "0"; for N>1, top of byte is N "1"
       bits followed by a "0"); bytes after the first have "10" at the top
       and go into the pushback queue. */
    if (uc <= 0x7f) {
      /* One byte of UTF-8 is needed. */
      ch = (int)uc;
    } else if (uc <= 0x7ff) {
      /* Two bytes of UTF-8 are needed. */
      state->chars[0] = (char)((uc & 0x3f) | 0x80);
      state->count = 1;
      ch = (int)((uc >> 6) | 0xc0);
    } else if (uc <= 0xffff) {
      /* Three bytes of UTF-8 are needed. */
      state->chars[0] = (char)((uc & 0x3f) | 0x80);
      state->chars[1] = (char)(((uc >> 6) & 0x3f) | 0x80);
      state->count = 2;
      ch = (int)((uc >> 12) | 0xe0);
    } else {
      /* Four bytes of UTF-8 are needed. */
      state->chars[0] = (char)((uc & 0x3f) | 0x80);
      state->chars[1] = (char)(((uc >> 6) & 0x3f) | 0x80);
      state->chars[2] = (char)(((uc >> 12) & 0x3f) | 0x80);
      state->count = 3;
      ch = (int)(((uc >> 18) & 0x7) | 0xf0);
    }  /* if */
  }  /* if */
have_ch:
  return ch;
}  /* getc_utf16 */

#endif /* UNICODE_SOURCE_SUPPORTED */

unsigned unicode_to_utf8(unsigned long uc,
                         unsigned char chars[4])
/*
Convert the Unicode code point uc to UTF-8.  Put the bytes of the UTF-8
representation in the array chars, and return the length (1-4).
*/
{
  unsigned len;

  if (uc <= 0x7f) {
    /* One byte of UTF-8 is needed. */
    len = 1;
    chars[0] = (unsigned char)uc;
  } else if (uc <= 0x7ff) {
    /* Two bytes of UTF-8 are needed. */
    len = 2;
    chars[0] = (unsigned char)((uc >> 6) | 0xc0);
    chars[1] = (unsigned char)((uc & 0x3f) | 0x80);
  } else if (uc <= 0xffff) {
    /* Three bytes of UTF-8 are needed. */
    len = 3;
    chars[0] = (unsigned char)((uc >> 12) | 0xe0);
    chars[1] = (unsigned char)(((uc >> 6) & 0x3f) | 0x80);
    chars[2] = (unsigned char)((uc & 0x3f) | 0x80);
  } else {
    /* Four bytes of UTF-8 are needed. */
    len = 4;
    chars[0] = (unsigned char)(((uc >> 18) & 0x7) | 0xf0);
    chars[1] = (unsigned char)(((uc >> 12) & 0x3f) | 0x80);
    chars[2] = (unsigned char)(((uc >> 6) & 0x3f) | 0x80);
    chars[3] = (unsigned char)((uc & 0x3f) | 0x80);
  }  /* if */
  return len;
}  /* unicode_to_utf8 */

#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE

a_boolean set_windows_locale(ARG_UNUSED a_const_char *locale_name)
/*
Set the locale to be used for multibyte character to Unicode conversion.
Return TRUE if the locale_name is invalid, FALSE otherwise.
*/
{
#if EDG_WIN32
  _locale_t	new_locale;

  new_locale = _create_locale(LC_ALL, locale_name);
  if (new_locale != NULL) {
    native_multibyte_locale = new_locale;
  }  /* if */
  return new_locale == NULL;
#else /* !EDG_WIN32 */
  /* Stub version of this routine used on non-Windows platforms.
     Returning TRUE causes any locale name to be considered invalid. */
  return TRUE;
#endif /* EDG_WIN32 */
}  /* set_windows_locale */


int unicode_to_multibyte_char(
                     ARG_UNUSED unsigned long uc,
                     unsigned char            chars[MAX_MULTIBYTE_CHAR_LENGTH],
                     a_boolean                *err)
/*
Convert the Unicode code point uc to a multibyte character sequence.  Put
the bytes of the multibyte representation in the array chars, and return
the length.  If the conversion could not be done, a "?" and length of 1 are
returned.  err is set to TRUE if the conversion failed, FALSE otherwise.

The EDG-supplied version of this routine only supports this capability when
EDG_WIN32 is TRUE (because it relies on Windows routines), in which case
the locale to be used for the multibyte encoding is specified by
system_default_locale.
*/
{
  int len;

  *err = FALSE;
#if EDG_WIN32
  /* Convert the Unicode character to a multibyte character in the specified
     locale. */
  if (_wctomb_s_l(&len, (char*)chars, MAX_MULTIBYTE_CHAR_LENGTH, (wchar_t)uc,
                  system_default_locale)) {
    /* The conversion failed.  Return "?". */
    chars[0] = '?';
    len = 1;
    *err = TRUE;
  }  /* if */
#else /* !EDG_WIN32 */
#if EDG_NATIVE_MULTIBYTE_TEST_MODE
  /* Always return an error. */
  chars[0] = '?';
  len = 1;
  *err = TRUE;
#else /* !EDG_NATIVE_MULTIBYTE_TEST_MODE */
  #error unicode_to_multibyte_char requires customization on non-Windows \
         platforms when using NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE.
#endif /* EDG_NATIVE_MULTIBYTE_TEST_MODE */
#endif /* EDG_WIN32 */
  return len;
}  /* unicode_to_multibyte_char */
    

char *multibyte_chars_to_utf8(a_const_char *str_ptr,
			      sizeof_t     *str_length,
			      a_boolean    *err)
/*
str_ptr points to a character string of str_length bytes containing multibyte
characters.  Convert the string to UTF-8 and return a pointer to the converted
string.  Update str_length to reflect the length in bytes of the new string.
Returns a pointer into the utf8_buffer text buffer.  If the identifier
contains a character that cannot be represented in Unicode, *err is set to
TRUE (FALSE otherwise).

The EDG-supplied version of this routine only supports this capability when
EDG_WIN32 is TRUE (because it relies on Windows routines), in which case
the locale to be used for the multibyte encoding is specified by
native_multibyte_locale.
*/
{
  a_const_char		*ptr;
  a_const_char		*after_str_end = str_ptr + *str_length;

  /* Clear the caller's error flag. */
  *err = FALSE;
  /* Allocate the buffer if it does not exist yet. */
  if (utf8_buffer == NULL) {
    /* min_buffer_size is just an estimate.  The buffer will be reallocated if
       the initial value is too small. */
    sizeof_t		min_buffer_size = *str_length * 2;
    utf8_buffer = alloc_text_buffer(min_buffer_size > 1024 ? min_buffer_size
                                                           : 1024);
  } else {
    reset_text_buffer(utf8_buffer);
  }  /* if */
  /* Make sure any shift states are reset. */
  mbc_scan_init();
  /* Go through the string and convert each multibyte sequence to a
     wide character.  Then convert each wide character to UTF-8. */
  for (ptr = str_ptr; ptr < after_str_end;) {
    unsigned long wc;
    int           mbclen;
    a_boolean     local_err;
    mbclen = mbc_to_wide_char(ptr, &wc, &local_err, /*is_native=*/TRUE);
    if (local_err) *err = TRUE;
    ptr += mbclen;
    if (wc <= 0x7f) {
      add_char_to_text_buffer(utf8_buffer, (char)wc);
    } else {
      /* Convert the Unicode value to UTF-8. */
      unsigned char arr[4];
      unsigned      utflen = unicode_to_utf8(wc, arr);
      for (unsigned i = 0; i < utflen; i++) {
        add_char_to_text_buffer(utf8_buffer, arr[i]);
      }  /* for */
    }  /* if */
  }  /* for */
  /* Add a null terminator. */
  add_char_to_text_buffer(utf8_buffer, '\0');
  /* Return the length (subtracting the null terminator). */
  *str_length = utf8_buffer->size - 1;
  return utf8_buffer->buffer;
}  /* multibyte_chars_to_utf8 */

#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */

unsigned long extract_character_from_string(a_const_char  *str,
                                            unsigned int  char_size)
/*
Extract a character of the given size from the given string and return its
value.  This routine deals with endianness, but not with the possibility of
multicharacter encodings (e.g., surrogate pairs in UTF-16 are not combined
into single character values).
*/
{
  unsigned long  wc = 0;
  unsigned char  ch;
  unsigned int   i;

  if (targ_little_endian) {
    for (i = 0; i < char_size; ++i) {
      ch = (unsigned char)str[(char_size - 1) - i];
      wc <<= targ_char_bit;
      wc |= ch;
    }  /* for */
  } else {
    for (i = 0; i < char_size; ++i) {
      ch = (unsigned char)str[i];
      wc <<= targ_char_bit;
      wc |= ch;
    }  /* for */
  }  /* if */
  return wc;
}  /* extract_character_from_string */


int ucn_to_utf16(unsigned long   ucn,
                 unsigned short  *encoding)
/*
Encode the given 32-bit character code as UTF-16 values stored in an array
pointed to by encoding.  Return the number of array elements used by the
encoding (never more than MAX_CHAR16_T_ENCODING_LENGTH), or zero if no valid
encoding could be achieved.  Note that this routine only handles the host-side
of the encoding: Target-size issues (such as endianness) are handled elsewhere
(e.g., in put_wide_char_into_string).
*/
{
  int  result;

  if (ucn <= 0xFFFF) {
    /* No need for a surrogate pair.  (The code points 0xD800 through 0xDFFF
       are normally reserved for surrogate pair encoding.  They aren't valid
       universal character names in C99 or C++.  This encoding routine just
       encodes them "as is", which might result in an invalid UTF-16 code.) */
    result = 1;
    encoding[0] = (unsigned short)ucn;
  } else {
    /* Form a surrogate pair. */
    if (ucn <= 0x10FFFF) {
      unsigned long high, low;
      result = 2;
      ucn -= 0x10000;
      low = 0xDC00 | (ucn & 0x3FF);
      high = 0xD800 | ((ucn >> 10) & 0x3FF);
      encoding[0] = (unsigned short)high;
      encoding[1] = (unsigned short)low;
    } else {
      /* UTF-16 cannot represent code points above 0x10FFFF. */
      result = 0;
    }  /* if */
  }  /* if */
  return result;
}  /* ucn_to_utf16 */


static inline void append_dir_name(a_text_buffer_ptr buf,
                                   a_const_char      *dir_name,
                                   a_boolean         normalize = TRUE)
/*
Add dir_name to the end of the directory name specified by buf.  If normalize
is true, special handling for the relative path delimiters "." and ".." is
applied to simplify the path.
*/
{
  a_const_char	*ptr = dir_name;
  a_const_char	*dir_start;
  int		length;
  a_boolean	starts_with_separator;

  /* If this is a UNC path, add one now as multiple are collapsed. */
  if (windows_paths_allowed &&
      is_dir_separator(dir_name[0]) && is_dir_separator(dir_name[1])) {
    add_char_to_text_buffer(buf, DIRECTORY_SEPARATOR);
  }  /* if */
  while (*ptr != '\0') {
    /* Skip past any delimiter characters. */
    starts_with_separator = is_dir_separator(*ptr);
    while (is_dir_separator(*ptr)) ptr++;
    /* Save the position of the start of the directory name. */
    dir_start = ptr;
    /* Find the end of the directory. */
    while (*ptr != '\0' && !is_dir_separator(*ptr)) increment_mbc_ptr(ptr);
    length = (int)(ptr - dir_start);
    if (normalize && length == 1 && *dir_start == '.') {
      /* "." for the current directory.  Ignore it. */
    } else if (normalize && length == 2 &&
               strncmp(dir_start, "..", 2) == 0) {
      /* ".." (parent directory).  Remove the last directory component from
         the buffer. */
      /* Get a pointer to the end of the buffer so far. */
      char	*buf_end = &buf->buffer[buf->size - 1];
      if (buf->size == 0) {
        /* We are already at the start of the buffer. */
      } else if (windows_paths_allowed && buf->size == 2 &&
                 has_drive_specification(buf->buffer)) {
        /* On Windows, we are back to something like "C:".  Don't go any
           further. */
      } else {
        /* Back up to the start of the previous directory component.  This
           actually needs to be done by scanning from the start of the
           string to handle multibyte characters. */
        a_const_char *last_dir_sep = NULL;
        a_const_char *ds_ptr;
        for (ds_ptr = &buf->buffer[0]; ds_ptr < buf_end;
            increment_mbc_ptr(ds_ptr)) {
          if (is_dir_separator(*ds_ptr)) last_dir_sep = ds_ptr;
        }  /* for */
        if (last_dir_sep != NULL) {
          buf->size -= (sizeof_t)(buf_end - last_dir_sep + 1);
        } else {
          /* There was no previous directory separator. */
          buf->size = 0;
        }  /* if */
      }  /* if */
    } else if (length > 0) {
      /* If there was a separator, or if we already have a directory name,
         add a separator now.  The separator needs to be suppressed at
         the start of a Windows file name that has a drive specification. */
      if (starts_with_separator || buf->size != 0) {
        add_char_to_text_buffer(buf, DIRECTORY_SEPARATOR);
      }  /* if */
      /* Add the directory name to the buffer. */
      add_to_text_buffer(buf, dir_start, (sizeof_t)(unsigned int)length);
    }  /* if */
  }   /* while */
}  /* append_dir_name */


static char *normalize_dir_name(a_const_char		*dir_name,
				a_text_buffer_ptr	buf,
				a_boolean		is_partial_file_name)
/*
Convert "dir_name" into a canonical form so that it can be compared with
other normalized directory names.

For example, if the current directory is "/a/b", then a directory named
"../c" will be normalized to "/a/c".  Note that even absolute path
names must be normalized as "/a/b/../c" should produce the same result
as "/a/c".

buf is a text buffer that is used to construct the normalized file name.

is_partial_file_name is TRUE if the file names are not known to be relative
to the current directory.
*/
{
  a_const_char *cwd = get_working_directory();

  reset_text_buffer(buf);
  if (!is_absolute_file_name(dir_name)) {
    if (!is_partial_file_name) {
      /* A relative file name.  Start with the current directory name. */
      append_dir_name(buf, cwd);
    }  /* if */
  } else {
#if __MICROSOFT_OS__
    /* If the path is absolute, but lacks a drive specification, use the
       drive from the current directory name. */
    if (!has_drive_specification(dir_name)) {
      check_assertion(has_drive_specification(cwd));
      add_to_text_buffer(buf, cwd, 2);
    }  /* if */
#endif /* __MICROSOFT_OS__ */
  }  /* if */
  /* Add the specified directory name. */
  append_dir_name(buf, dir_name);
  /* Terminate the string. */
  add_char_to_text_buffer(buf, '\0');
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("normalize_dir_name")) {
    fprintf(f_debug, "normalize_dir_name in=%s out=%s\n", dir_name,
            buf->buffer);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return buf->buffer;
}  /* normalize_dir_name */


STATIC_THREAD a_text_buffer_ptr
		dir_buffer1;
STATIC_THREAD a_text_buffer_ptr
		dir_buffer2;
				/* Text buffers used to construct a
				   normalized directory name. */


int compare_dir_names(a_const_char *dir1,
		      a_const_char *dir2,
		      a_boolean	   is_partial_file_name)
/*
Compare the directory names specified by dir1 and dir2.  Return zero if
they are the same.

is_partial_file_name is TRUE if the file names are not known to be relative
to the current directory.
*/
{
  int	result;

  /* The first time this routine is called, allocate text buffers used
     to construct the normalized directory names. */
  if (dir_buffer1 == NULL) {
    dir_buffer1 = alloc_text_buffer(128);
  }  /* if */
  if (dir_buffer2 == NULL) {
    dir_buffer2 = alloc_text_buffer(128);
  }  /* if */
  dir1 = normalize_dir_name(dir1, dir_buffer1, is_partial_file_name);
  dir2 = normalize_dir_name(dir2, dir_buffer2, is_partial_file_name);
  result = compare_file_chars(dir1, dir2);
#if UNIQUE_FILE_IDENTIFIER_AVAILABLE
  if (result != 0 && !is_partial_file_name) {
    /* If unique file identifiers are available, and we know that we are
       dealing with entire directory names, compare the unique file
       identifiers for the directories. */
    a_unique_file_id	id1;
    a_unique_file_id	id2;
    get_unique_id_for_file(dir1, &id1);
    get_unique_id_for_file(dir2, &id2);
    result = same_unique_file_ids(&id1, &id2) ? 0 : 1;
  }  /* if */
#endif /* UNIQUE_FILE_IDENTIFIER_AVAILABLE */
  return result;
}  /* compare_dir_names */


char *normalize_file_name(a_const_char *file_name)
/*
Normalize "file_name" by converting it into a canonical form.  For
example, if the file name is "/a/b/../c", the normalized name will
be "/a/c".

The string returned points to the contents of a text buffer.  The buffer will
be overwritten by subsequent calls of this routine, compare_dir_names, or
resolve_file_name.
*/
{
  char *result;

  /* Allocate a directory buffer if not already allocated. */
  if (dir_buffer1 == NULL) {
    dir_buffer1 = alloc_text_buffer(128);
  }  /* if */
  result = normalize_dir_name(file_name, dir_buffer1,
                              /*is_partial_file_name=*/FALSE);
  return result;
}  /* normalize_file_name */


static a_const_char* resolve_file_name(a_const_char *file_name,
                     /* Defaulted: */  a_const_char *dir_name)
/*
Form an absolute file name using the given file name, directory name, and the
current working directory.

When dir_name is NULL this differs from normalize_file_name only in that it
preserves relative path delimiters (e.g., "..") for the operating system to
resolve.

The string returned may point to the contents of a text buffer.  The buffer
will be overwritten by subsequent calls of this routine, compare_dir_names, or
normalize_file_name.
*/
{
  a_const_char *result;

  /* If the given file name is not an absolute path, prepend either the current
     working directory or the specified root directory. */
  if (!is_absolute_file_name(file_name)) {
    /* Allocate a directory buffer if not already allocated. */
    if (dir_buffer1 == NULL) {
      dir_buffer1 = alloc_text_buffer(128);
    }  /* if */
    /* Append the file to the directory information in dir_buffer1. */
    reset_text_buffer(dir_buffer1);
    if (dir_name == NULL || !is_absolute_file_name(dir_name)) {
      /* Prepend the current working directory if no directory name was given
         or if the directory name given is itself relative to the current
         working directory. */
      append_dir_name(dir_buffer1, get_working_directory(),
                      /*normalize=*/FALSE);
    }  /* if */
    if (dir_name != NULL) {
      /* Append the provided directory information.  At this point the buffer
         should contain nothing if the directory name is an absolute path;
         otherwise, it should contain the current working directory. */
      append_dir_name(dir_buffer1, dir_name, /*normalize=*/FALSE);
    }  /* if */
    append_dir_name(dir_buffer1, file_name, /*normalize=*/FALSE);
    /* Terminate the string. */
    add_char_to_text_buffer(dir_buffer1, '\0');
    result = dir_buffer1->buffer;
  } else {
    result = file_name;
  }  /* if */
  return result;
}  /* resolve_file_name */

#if !STANDALONE_UTILITY_PROGRAM

int f_compare_file_names(a_const_char	*file1,
	 		 a_const_char	*file2,
		         a_boolean	ignore_delimiters,
			 a_boolean	is_partial_file_name,
			 a_boolean      in_general_memory)
/*
Return zero if file1 and file2 name the same file.  ignore_delimiters
is TRUE if the file names are from #include directives and still have
the '"' or '<' delimiters.  is_partial_file_name is TRUE if the
file names are not known to be relative to the current directory.  If
in_general_memory is TRUE, any needed allocations will occur in general memory.
Otherwise, allocations will occur in the intermediate language memory region.
*/
{
  a_const_char	*start1 = file1;
  a_const_char	*start2 = file2;
  a_const_char	*file_start1;
  a_const_char	*file_start2;
  char		*end1 = NULL;
  char		*end2 = NULL;
  a_boolean	match = FALSE;
  char		saved_delim1 = '\0';
  char		saved_delim2 = '\0';

  check_assertion(in_general_memory || mem_region_table != NULL);
  /* If we are ignoring delimiters, temporarily replace the trailing
     delimiter with a null. */
  if (ignore_delimiters) {
    end1 = (char *)start1 + strlen(file1) - 1;
    saved_delim1 = *end1;
    *end1 = '\0';
    end2 = (char *)start2 + strlen(file2) - 1;
    saved_delim2 = *end2;
    *end2 = '\0';
    /* Increment the starting point past the delimiters. */
    start1++;
    start2++;
  }  /* if */
  /* Find the start of the actual file name component of the two files. */
  file_start1 = start_of_file_name(start1);
  file_start2 = start_of_file_name(start2);
  if (compare_file_chars(file_start1, file_start2) == 0) {
    /* Only the directory names only if the file name components match. */
    char	*dir1;
    char	*dir2;
    /* Normalize the directory names so that "./x.h" and "x.h" will
       compare equal. */
    dir1 = f_directory_of(start1, in_general_memory);
    dir2 = f_directory_of(start2, in_general_memory);
    if (compare_dir_names(dir1, dir2, is_partial_file_name) == 0) {
      match = TRUE;
    }  /* if */
  }  /* if */
  if (ignore_delimiters) {
    /* Restore the original delimiter characters. */
    *end1 = saved_delim1;
    *end2 = saved_delim2;
  }  /* if */
  /* Convert the boolean result into a strcmp-like result value. */
  return match ? 0 : 1;
}  /* f_compare_file_names */

#endif /* !STANDALONE_UTILITY_PROGRAM */

#if __MICROSOFT_OS__ && EDG_WIN32 && UNICODE_SOURCE_SUPPORTED

static unsigned short upper_case_utf16_char(unsigned short utf16_char)
/*
Return the locale-insensitive upper-case mapping of the UTF-16 code unit
utf16_char or utf16_char itself when it has no upper-case mapping.

Note: the CharUpperW function performs this mapping, but it is deliberately not
used here to avoid depending on User32.dll.
*/
{
  if (utf16_char < 0x80) {
    /* ASCII: which is by far the common case for file names. */
    if (utf16_char >= (unsigned short)'a' &&
        utf16_char <= (unsigned short)'z') {
      utf16_char -= (unsigned short)('a' - 'A');
    }  /* if */
  } else if (utf16_char < 0xD800 || utf16_char > 0xDFFF) {
    WCHAR  source_char;
    WCHAR  upper_case_char;

    /* Not half of a surrogate pair, so this code unit is a complete
       character and can be mapped on its own. */
    source_char = (WCHAR)utf16_char;
    if (LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE, &source_char,
                     /*cchSrc=*/1, &upper_case_char, /*cchDest=*/1) == 1) {
      utf16_char = (unsigned short)upper_case_char;
    }  /* if */
  }  /* if */
  return utf16_char;
}  /* upper_case_utf16_char */


int compare_file_chars_case_insensitive(a_const_char *file1,
                                        a_const_char *file2)
/*
Compare the file names specified by "file1" and "file2" in a
case-insensitive manner.  Return zero if they are the same.  "file1" and
"file2" are expected to be encoded in UTF-8.  Each UTF-8 character will be
converted to UTF-16, and the comparison will be performed using the
upper-case mapping each UTF-16 code unit.
*/
{
  int             result = 0;
  unsigned long   unicode_char;
  a_boolean       err;
  int             num_utf8_bytes, num_utf16_chars;
  unsigned short  utf16_chars[2];
  unsigned char   *p_file1_utf8_char = (unsigned char *)file1;
  unsigned char   *p_file2_utf8_char = (unsigned char *)file2;
  unsigned short  file1_utf16_char, file1_next_utf16_char = 0;
  unsigned short  file2_utf16_char, file2_next_utf16_char = 0;

  check_assertion(p_file1_utf8_char != NULL && p_file2_utf8_char != NULL);
  for (;;) {
    /* Obtain the next UTF-16 code unit from "file1". */
    if (file1_next_utf16_char == 0) {
      if (*p_file1_utf8_char < 0x80) {
        /* This is an ASCII character. */
        file1_utf16_char = (unsigned short)*p_file1_utf8_char;
        p_file1_utf8_char++;
      } else {
        /* Convert a UTF-8 character to a single Unicode code point. */
        num_utf8_bytes = mbc_to_wide_char((char *)p_file1_utf8_char,
                                          &unicode_char, &err,
                                          /*is_native=*/FALSE);
        /* Convert that to either one UTF-16 value or a pair of surrogates. */
        num_utf16_chars = ucn_to_utf16(unicode_char, utf16_chars);
        check_assertion(num_utf16_chars <= 2);
        file1_utf16_char = utf16_chars[0];
        if (num_utf16_chars == 2) file1_next_utf16_char = utf16_chars[1];
        p_file1_utf8_char += num_utf8_bytes;
      }  /* if */
    } else {
      /* The previous UTF-16 code unit was the first of a pair of
         surrogates. */
      file1_utf16_char = file1_next_utf16_char;
      file1_next_utf16_char = 0;
    }  /* if */
    /* Obtain the next UTF-16 code unit from "file2". */
    if (file2_next_utf16_char == 0) {
      if (*p_file2_utf8_char < 0x80) {
        /* This is an ASCII character. */
        file2_utf16_char = (unsigned short)*p_file2_utf8_char;
        p_file2_utf8_char++;
      } else {
        /* Convert a UTF-8 character to a single Unicode code point. */
        num_utf8_bytes = mbc_to_wide_char((char *)p_file2_utf8_char,
                                          &unicode_char, &err,
                                          /*is_native=*/FALSE);
        /* Convert that to either one UTF-16 value or a pair of surrogates. */
        num_utf16_chars = ucn_to_utf16(unicode_char, utf16_chars);
        check_assertion(num_utf16_chars <= 2);
        file2_utf16_char = utf16_chars[0];
        if (num_utf16_chars == 2) file2_next_utf16_char = utf16_chars[1];
        p_file2_utf8_char += num_utf8_bytes;
      }  /* if */
    } else {
      /* The previous UTF-16 code unit was the first of a pair of
         surrogates. */
      file2_utf16_char = file2_next_utf16_char;
      file2_next_utf16_char = 0;
    }  /* if */
    if (file1_utf16_char == 0 || file2_utf16_char == 0) {
      /* The end of either of the names has been reached. */
      result = file2_utf16_char - file1_utf16_char;
      break;
    } else if (file1_utf16_char != file2_utf16_char) {
      /* The UTF-16 code units are different.  Compare their
         locale-insensitive upper-case mappings. */
      file1_utf16_char = upper_case_utf16_char(file1_utf16_char);
      file2_utf16_char = upper_case_utf16_char(file2_utf16_char);
      if (file1_utf16_char != file2_utf16_char) {
        /* The upper-case mappings of the UTF-16 code units are different. */
        result = file2_utf16_char - file1_utf16_char;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* compare_file_chars_case_insensitive */

#endif /* __MICROSOFT_OS__ && EDG_WIN32 && UNICODE_SOURCE_SUPPORTED */

#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE

static a_text_buffer_ptr utf8_to_multibyte_char(a_const_char *str)
/*
Convert "str" from UTF-8 to the native multibyte characters.  Return a
pointer to a text buffer containing the converted string.  The text buffer
will be reused on the next call to this routine, so the contents can only
be used until that point.
*/
{
  a_const_char *p;
  int          len;

  /* Allocate the buffer if it does not exist yet. */
  if (mbc_buffer == NULL) {
    mbc_buffer = alloc_text_buffer(1024);
  } else {
    reset_text_buffer(mbc_buffer);
  }  /* if */
  for (p = str; *p != '\0'; p += len) {
    if ((unsigned char)*p <= 0x7f) {
      /* For the typical case, just copy the character to the text buffer. */
      len = 1;
      add_char_to_text_buffer(mbc_buffer, *p);
    } else {
      /* Convert a UTF-8 character into a wide character. */
      int           mb_len;
      int           i;
      a_boolean     err;
      unsigned long wc;
      unsigned char arr[MAX_MULTIBYTE_CHAR_LENGTH];
      len = mbc_to_wide_char(p, &wc, (a_boolean*)NULL, /*is_native=*/FALSE);
      /* Convert the Unicode character to a native multibyte
         character sequence.  "?" will be returned in "arr" on error. */
      mb_len = unicode_to_multibyte_char(wc, arr, &err);
      for (i = 0; i < mb_len; i++) {
        add_char_to_text_buffer(mbc_buffer, arr[i]);
      }  /* if */
    }  /* if */
  }  /* for */
  /* Terminate the buffer. */
  add_char_to_text_buffer(mbc_buffer, '\0');
  return mbc_buffer;
}  /* utf8_to_multibyte_char */

#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */

void write_file_name_to_text_buffer(
                                   a_const_char     *name,
                                   a_text_buffer_ptr buffer,
                                   a_boolean         process_escapes,
                                   a_boolean         escape_nonprintable_chars)
/*
Write out the null-terminated file name "name" to the specified buffer.
If process_escapes is TRUE, an escape is added for quotes and backslashes.
If escape_nonprintable_chars is TRUE, nonprintable characters will be
put out using escape sequences.  Escape processing is generally suppressed
for names appearing in error messages, so that multibyte characters will
be output without escapes.  Escape processing is done when outputting names
in preprocessed output and similar contexts.  The result is not
null-terminated.
*/
{
  a_const_char  *p;
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
  a_boolean	is_native = FALSE;
  a_boolean	err;
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
#if EDG_WIN32
  /* In case the native multibyte locale has been changed (e.g., by the
     setlocale pragma) set it back to the system default locale for purposes
     of file name translation. */
  _locale_t	saved_locale = native_multibyte_locale;
  native_multibyte_locale = system_default_locale;
#endif /* EDG_WIN32 */
  /* File names are stored internally in UTF-8.  If the default Unicode
     source kind is usk_none, convert the file name to the system default
     locale character set. */
  if (DEFAULT_UNICODE_SOURCE_KIND == usk_none) {  /*lint !e506*/
    a_text_buffer_ptr	buf;
    buf = utf8_to_multibyte_char(name);
    name = buf->buffer;
    is_native = TRUE;
  }  /* if */
#else /* !NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
  name = file_name_in_external_encoding(name);
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
  /*lint --e{850} p modified in loop */
  for (p = name; *p != '\0'; p++) {
    char ch = *p;
    if (!escape_nonprintable_chars || isprint((unsigned char)ch)) {
      int	ch_len;
      /* If the character is printable, or if we are not escaping nonprintable
         characters, emit the character normally.  This is done so that
         characters from extended character sets and multibyte characters will
         be output as expected and not as octal escapes.  We have to assume
         that each such character occupies a single position in the length
         returned. */
      if (process_escapes && (ch == '"' || ch == '\\')) {
        add_char_to_text_buffer(buffer, '\\');
      }  /* if */
      /* Output all of the characters of a multibyte sequence so that
         the length returned will be correct.  Note that err and
         is_native are only used by the mbc_length_full macro when
         MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED is TRUE. */
      for (ch_len = mbc_length_full(p, &err, is_native);
           ch_len > 0; ch_len--, p++) {
        add_char_to_text_buffer(buffer, *p);
      }  /* for */
      /* Decrement p because it will be incremented at the end of the loop. */
      p--;
    } else if (ch == '\n') {
      /* Put out newline as \n. */
      add_string_to_text_buffer(buffer, "\\n");
    } else {
      /* Unprintable characters: put out as \ooo. */
      unsigned int     encoded =
                         (unsigned int)(ch&((1<<targ_host_string_char_bit)-1));
      Small_string<20> char_buffer(left_pad(3, '0', octal_view_of(encoded)));

      add_string_to_text_buffer(buffer, char_buffer.as_temp_characters());
    }  /* if */
  }  /* for */
#if EDG_WIN32 && NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
  /* Restore the original locale. */
  native_multibyte_locale = saved_locale;
#endif /* EDG_WIN32 && NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
}  /* write_file_name_to_text_buffer */


static
a_text_buffer_ptr f_format_file_name(a_const_char  *name,
                                     a_boolean     process_escapes,
                                     a_boolean     escape_nonprintable_chars)
/*
Format the null-terminated file name "name" and return a pointer to a
text buffer containing the result.  If process_escapes is TRUE, an
escape is added for quotes and backslashes.  If escape_nonprintable_chars
is TRUE, nonprintable characters will be put out using escape
sequences.  Escape processing is generally suppressed for names
appearing in error messages, so that multibyte characters will be
output without escapes.  Escape processing is done when outputting
names in preprocessed output and similar contexts.  This routine is
used (directly or by routines such as write_file_name) to write out the
file name in #line directives error messages, etc.  The text buffer will
be reused on the next call to this routine (or format_source_file_name), so the
contents can only be used until that point.
*/
{
  if (format_file_name_buffer == NULL) {
    /* Allocate a buffer into which the file name will be written. */
    format_file_name_buffer = alloc_text_buffer(256);
  }  /* if */
  reset_text_buffer(format_file_name_buffer);
  write_file_name_to_text_buffer(name, format_file_name_buffer,
                                 process_escapes,
                                 escape_nonprintable_chars);
  add_char_to_text_buffer(format_file_name_buffer, '\0');
  return format_file_name_buffer;
}  /* f_format_file_name */


char *format_file_name(a_const_char *name)
/*
Return a pointer to a version of the file name "name" formatted for display
purposes.  This returns a pointer into a text buffer used by
f_format_file_name.  The pointer returned must be used before that routine
is called again.
*/
{
  a_text_buffer_ptr	buf;

  buf = f_format_file_name(name,
                           /*process_escapes=*/FALSE,
                           /*escapes_nonprintable_chars=*/FALSE);
  return buf->buffer;
}  /* format_file_name */


char *format_source_file_name(a_source_file_ptr sfp,
                              a_boolean         use_name_as_written,
                              a_boolean         quote_file_name)
/*
Similar to format_file_name, except uses additional information within sfp to
provide additional information.  Return a pointer to a version of the file name
formatted for display purposes.  This returns a pointer into a text buffer used
by f_format_file_name.  The pointer returned must be used before either this or
that routine is called again.  If use_name_as_written is TRUE, use the
name_as_written field of sfp, otherwise use the file_name field.  If
quote_file_name is TRUE, add quotes around the formatted file name where
appropriate.
*/
{
  a_const_char      *name;

  if (format_file_name_buffer == NULL) {
    /* Allocate a buffer into which the file name will be written. */
    format_file_name_buffer = alloc_text_buffer(256);
  }  /* if */
  reset_text_buffer(format_file_name_buffer);
  name = use_name_as_written ? sfp->name_as_written : sfp->file_name;

  a_boolean display_module_info = (sfp->assoc_module != NULL);
  if (display_module_info) {
    a_module_ptr mod = sfp->assoc_module;

    /* FIXME: We probably want to add error codes to aid in translation. */
    switch (mod->kind) {
      case mk_none:
        add_string_to_text_buffer(format_file_name_buffer, "unknown module");
        break;
      case mk_header_unit:
        { a_boolean     is_sys_include =
                                       mod->variant.header_unit.is_sys_include;
          a_string_view header_name = header_unit_name_of(mod);

          add_string_to_text_buffer(format_file_name_buffer, "header unit ");
          if (is_sys_include) {
            add_char_to_text_buffer(format_file_name_buffer, '<');
          } else {
            add_char_to_text_buffer(format_file_name_buffer, '"');
          }  /* if */
          add_string_to_text_buffer(format_file_name_buffer,
                                    header_name.start());
          if (is_sys_include) {
            add_char_to_text_buffer(format_file_name_buffer, '>');
          } else {
            add_char_to_text_buffer(format_file_name_buffer, '"');
          }  /* if */
        }
        break;
      case mk_unit:
      case mk_unit_partition:
        { a_string mod_name = module_full_name_of(mod);

          add_string_to_text_buffer(format_file_name_buffer, "module ");
          add_char_to_text_buffer(format_file_name_buffer, '"');
          add_string_to_text_buffer(format_file_name_buffer,
                                    mod_name.as_temp_characters());
          add_char_to_text_buffer(format_file_name_buffer, '"');
        }
        break;
      default_is_unexpected();
    }  /* switch */
    add_string_to_text_buffer(format_file_name_buffer, " (");
    quote_file_name = TRUE;
  }  /* if */
  if (quote_file_name) {
    add_char_to_text_buffer(format_file_name_buffer, '"');
  }  /* if */
  write_file_name_to_text_buffer(name, format_file_name_buffer,
                                 /*process_escapes=*/FALSE,
                                 /*escape_nonprintable_chars=*/FALSE);
  if (quote_file_name) {
    add_char_to_text_buffer(format_file_name_buffer, '"');
  }  /* if */
  if (display_module_info) {
    add_char_to_text_buffer(format_file_name_buffer, ')');
  }  /* if */
  add_char_to_text_buffer(format_file_name_buffer, '\0');
  return format_file_name_buffer->buffer;
}  /* format_source_file_name */


void write_file_name(a_const_char *name,
                     FILE         *f_output,
                     a_boolean    process_escapes,
                     a_boolean    escape_nonprintable_chars)
/*
Write out the null-terminated file name "name" to the output file f_output.
If process_escapes is TRUE, an escape is added for quotes and backslashes.
If escape_nonprintable_chars is TRUE, nonprintable characters will be
put out using escape sequences.  Escape processing is generally suppressed
for names appearing in error messages, so that multibyte characters will
be output without escapes.  Escape processing is done when outputting names
in preprocessed output and similar contexts.  The caller must put out
surrounding quotes if they are needed.
*/
{
  a_text_buffer_ptr buf;

  buf = f_format_file_name(name, process_escapes,
                           escape_nonprintable_chars);
  fputs(buf->buffer, f_output);
}  /* write_file_name */

#if EDG_WIN32 && NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE

static char *get_system_default_locale_name(void)
/*
Return a pointer to a locale name that can be used to create a locale
object for the system default locale.

If the environment variable EDG_DEFAULT_SYSTEM_LOCALE is defined, that
value is returned.  Otherwise, this routine separately fetches the
language, country, and codepage and then constructs a locale name with the
components.  For a typical U.S. system, this will return
"English_USA.1252".  A Japanese system would return "Japanese_JPN.932".
*/
{
#define TMP_BUF_SIZE 256
  char     buf[TMP_BUF_SIZE];
  sizeof_t chars;
  char     *locale_from_env = getenv("EDG_DEFAULT_SYSTEM_LOCALE");

  locale_name_buffer = alloc_text_buffer(128);
  
  if (locale_from_env != NULL) {
    /* Copy the value of the environment variable. */
    add_to_text_buffer(locale_name_buffer, locale_from_env,
                       strlen(locale_from_env) + 1);
  } else {
    /* Each call returns the size of the resulting string, including the
       null terminator. */
    chars = GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_SENGLANGUAGE,
                          buf, TMP_BUF_SIZE);
    check_assertion(chars != 0);
    add_to_text_buffer(locale_name_buffer, buf, chars-1);
    chars = GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_SABBREVCTRYNAME,
                          buf, TMP_BUF_SIZE);
    check_assertion(chars != 0);
    add_char_to_text_buffer(locale_name_buffer, '_');
    add_to_text_buffer(locale_name_buffer, buf, chars-1);
    chars = GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE,
                          buf, TMP_BUF_SIZE);
    check_assertion(chars != 0);
    add_char_to_text_buffer(locale_name_buffer, '.');
    add_to_text_buffer(locale_name_buffer, buf, chars);
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("locale")) {
    fprintf(f_debug, "System default locale is %s\n",
            locale_name_buffer->buffer);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return locale_name_buffer->buffer;
#undef TMP_BUF_SIZE
}  /* get_system_default_locale_name */

#endif /* EDG_WIN32 && NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */

#if EDG_WIN32

char *conv_wide_to_utf8(wchar_t *wide_str)
/*
Convert a wide character string to a UTF-8 encoded string and return it
in a temporary buffer.
*/
{
  wchar_t       *ptr = wide_str;

  check_assertion(wide_str);
  /* Allocate the buffer if it does not exist yet. */
  if (conv_utf8_buffer == NULL) {
    /* Estimate the length, it's OK to waste some memory because this buffer
       will be used repeatedly and the result is either thrown away or
       copied into an appropriately sized buffer.  Furthermore, if this
       still isn't enough memory, the buffer will be expanded below. */
    sizeof_t      wide_buffer_size = wcslen(wide_str) + 1;
    sizeof_t      utf8_buffer_size = wide_buffer_size * 4;

    conv_utf8_buffer = alloc_text_buffer(utf8_buffer_size > 1024 ?
                                                utf8_buffer_size : 1024);
  } else {
    reset_text_buffer(conv_utf8_buffer);
  }  /* if */
  while (*ptr) {
    /* Convert the Unicode value to UTF-8. */
    if (*ptr <= 0x7f) {
      add_char_to_text_buffer(conv_utf8_buffer, (char)*ptr);
    } else {
      unsigned char arr[4];
      unsigned      utflen = unicode_to_utf8(*ptr, arr);
      for (unsigned i = 0; i < utflen; i++) {
        add_char_to_text_buffer(conv_utf8_buffer, arr[i]);
      }  /* for */
  }  /* if */
    ++ptr;
  }  /* while */
  /* Add a null terminator. */
  add_char_to_text_buffer(conv_utf8_buffer, '\0');
  return conv_utf8_buffer->buffer;
}  /* conv_wide_to_utf8 */

#endif /* EDG_WIN32 */

#if !STANDALONE_UTILITY_PROGRAM
#if EDG_WIN32

a_const_char *win32_error_to_str(an_ms_dword err_code)
/*
Use the system routine FormatMessageA to get the message for "err_code".  If
the system routine fails for any reason, return the string "unknown error".
In non-error cases, this routine returns a pointer to the temp_text_buffer,
so the result must be used before the buffer is reused.
*/
{
  unsigned long chars_written;
  a_const_char  *result;

  ensure_temp_text_buffer_space(256);
  chars_written = FormatMessageA(
                      FORMAT_MESSAGE_FROM_SYSTEM, /*lpSource=*/NULL, err_code,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
                      temp_text_buffer, /*nSize=*/256, /*Arguments=*/NULL);

  if (chars_written == 0) {
    /* The err_code may not have an associated message.  Return "unknown
       error", which is only slightly better than returning nothing. */
    result = error_text(ec_no_error);
  } else {
    result = temp_text_buffer;
  }  /* if */

  return result;
}  /* win32_error_to_str */

#if CPPCLI_ENABLING_POSSIBLE

static void get_clr_runtime_directory(wchar_t  *dir_name, 
                                      sizeof_t *dir_name_size)
/*
Gets the installation directory of the common language runtime (CLR).

dir_name is the buffer in which the directory name is returned.  dir_name_size
is the length of the dir_name buffer.
*/
{
  ICLRMetaHostPolicy *cmhpp = NULL;
  ICLRRuntimeInfo    *crip = NULL;
  HRESULT            hr = E_FAIL;
  DWORD              dword_dir_name_size = (DWORD)*dir_name_size;
  static             wchar_t runtime_directory[_MAX_DIR] = {0};

  /* Check the cached name first.  The name won't change while running. */
  if (runtime_directory[0] != L'\0') {
    wcscpy_s(dir_name, *dir_name_size, runtime_directory);
    goto end_of_routine;
  }  /* if */
  /* Get the ICLRMetaHostPolicy interface to query for the preferred CLR
     runtime version based on the available versions that are installed or
     loaded. */
  hr = CLRCreateInstance(CLSID_CLRMetaHostPolicy, IID_ICLRMetaHostPolicy, 
                         (LPVOID*)(&cmhpp));
  if (FAILED(hr)) {
    hresult_catastrophe("CLRCreateInstance");
  }  /* if */
  check_assertion(cmhpp != NULL);
  /* First try to get the version of the runtime specified by the application
     config file (app.exe.config). */
  cmhpp->GetRequestedRuntime(
                          METAHOST_POLICY_USE_PROCESS_IMAGE_PATH,
                          /*pwzBinary=*/NULL, /*pCfgStream=*/NULL, 
                          /*pwzVersion=*/NULL, /*pcchVersion=*/NULL, 
                          /*pwzImageVersion=*/NULL, /*pcchImageVersion=*/NULL,
                          /*pdwConfigFlags=*/NULL, IID_ICLRRuntimeInfo, 
                          (LPVOID*)(&crip));
  if (FAILED(hr) || crip == NULL) {
#define CLR_VERSION_BUFFER_SIZE 128
    wchar_t            version_buffer[CLR_VERSION_BUFFER_SIZE];
    DWORD              version_size = CLR_VERSION_BUFFER_SIZE;
#undef CLR_VERSION_BUFFER_SIZE
    
    wcscpy(version_buffer, CLR_FALLBACK_VERSION);
    /* Fall back on the version of the runtime specified by CLR_VERSION. */
    cmhpp->GetRequestedRuntime(
                          (METAHOST_POLICY_FLAGS)
                          (METAHOST_POLICY_USE_PROCESS_IMAGE_PATH | 
                           METAHOST_POLICY_APPLY_UPGRADE_POLICY),
                          /*pwzBinary=*/NULL, /*pCfgStream=*/NULL, 
                          version_buffer, &version_size, 
                          /*pwzImageVersion=*/NULL, /*pcchImageVersion=*/NULL,
                          /*pdwConfigFlags=*/NULL, IID_ICLRRuntimeInfo, 
                          (LPVOID*)(&crip));
    if (FAILED(hr) || crip == NULL) {
      hresult_catastrophe("ICLRMetaHostPolicy::GetRequestedRuntime");
    }  /* if */
  }  /* if */
  check_assertion(crip != NULL);
  hr = crip->GetRuntimeDirectory(dir_name, &dword_dir_name_size);
  if (FAILED(hr)) {
    hresult_catastrophe("ICLRRuntimeInfo::GetRuntimeDirectory");
  }  /* if */
  crip->Release();
  cmhpp->Release();
  /* Cache the value for next time. */
  wcscpy_s(runtime_directory, _MAX_DIR, dir_name);
end_of_routine:
  return;
}  /* get_clr_runtime_directory */


a_const_char *com_error_to_str(void)
/*
Use the com facility GetErrorInfo to get a description of the last com failure,
returning "unknown error" if no description is available.  In some cases
this routine returns a pointer to a temporary buffer, in which case the result
must be used before the buffer (temp_text_buffer) is overwritten.
*/
{
  HRESULT      hr;
  IErrorInfo   *error_info = NULL;
  a_const_char *result = NULL;
  BSTR         description = NULL;

  hr = GetErrorInfo(0, &error_info);
  /* According to MSDN, GetErrorInfo returns either S_OK or S_FALSE. */
  check_assertion(hr == S_OK || hr == S_FALSE);
  if (hr == S_OK) {
    hr = error_info->GetDescription(&description);
    if (SUCCEEDED(hr)) {
      /* "description" may contain embedded NULLs, but those are ignored
         because we wouldn't know how to format the text anyway. */
      result = conv_wide_to_utf8(description);
      SysFreeString(description);
    }  /* if */
    error_info->Release();
  }  /* if */
  if (result == NULL) {
    /* Either the API didn't set the error info, or some other error
       occurred.  Return "unknown error", which is only slightly better than
       returning nothing. */
    result = error_text(ec_no_error);
  }  /* if */
  return result;
}  /* com_error_to_str */

#endif /* CPPCLI_ENABLING_POSSIBLE */

#endif /* EDG_WIN32 */

static void init_module_search_path(void)
/*
Complete the initialization of the module search path.  Because this is called
after the command line is processed, some directories may already be on the
module search path (via the option --modules_directory).  The final search path
will include, in this order:
  - Current directory
  - Directories specified from the --modules_directory option
  - EDG_MODULES_PATH environment variable
*/
{
  char       *env_path;

  /* The current directory is the first place we search, so prepend it. */
  add_to_front_of_include_search_path(get_working_directory(),
                                      &module_search_path,
                                      &end_module_search_path);
  env_path = getenv("EDG_MODULES_PATH");
  if (env_path != NULL) {
    add_to_specified_include_search_path(env_path, FALSE,
                                         &module_search_path,
                                         &end_module_search_path);
  }  /* if */
}  /* init_module_search_path */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void init_assembly_search_path(void)
/*
Complete the initialization of the assembly search path.  Because this is
called after the command line is processed, some directories may already
be on the assembly search path (via the option --using_directory).  The
final search path will include, in this order:
  - Current directory
  - .NET system directory (if we haven't seen --no_using_framework_directory)
  - Directories specified from the --using_directory option
  - EDG_CPPCLI_PORTABLE_ASSEMBLY_PATH environment variable (in some configs)
  - CPPCLI_PORTABLE_ASSEMBLY_PATH (in some configurations)
  - Directories from the environment variable LIBPATH
*/
{
  char       *libpath;
  char       *current_path;
  char       *semicolon;
  size_t     libpath_size;

#if EDG_WIN32 && CPPCLI_ENABLING_POSSIBLE
  if (using_framework_directory) {
    /* By default, the directory that the .NET runtime is installed in is
       included in the search when looking for referenced assemblies.  A
       (rarely used) command line switch disables including this directory
       in the search path. */
    wchar_t       clr_directory_wide[_MAX_DIR];
    char          *temp_buffer;
    char          *clr_directory_utf8;
    size_t        clr_directory_utf8_size;
    sizeof_t      length_wide = _MAX_DIR;

    get_clr_runtime_directory(clr_directory_wide, &length_wide);
    temp_buffer = conv_wide_to_utf8(clr_directory_wide);
    /* Copy the directory name to general memory. */
    clr_directory_utf8_size = strlen(temp_buffer) + 1;
    clr_directory_utf8 = alloc_general(clr_directory_utf8_size);
    strcpy(clr_directory_utf8, temp_buffer);
    add_to_front_of_include_search_path(clr_directory_utf8,
                                        &assembly_search_path,
                                        &end_assembly_search_path);  
  }  /* if */
#endif /* EDG_WIN32 && CPPCLI_ENABLING_POSSIBLE */
  /* The current directory is the first place we search, so prepend it. */
  add_to_front_of_include_search_path(get_working_directory(),
                                      &assembly_search_path,
                                      &end_assembly_search_path);
#if READ_CPPCLI_PORTABLE_ASSEMBLIES
  /* Make it easy to find portable assemblies on non-Windows systems. */
  { char *pa_path = getenv("EDG_CPPCLI_PORTABLE_ASSEMBLY_PATH");
    if (pa_path != NULL) {
      add_to_specified_include_search_path(pa_path, FALSE,
                             &assembly_search_path, &end_assembly_search_path);
    }  /* if */
  }
#ifdef CPPCLI_PORTABLE_ASSEMBLY_PATH
  add_to_specified_include_search_path(CPPCLI_PORTABLE_ASSEMBLY_PATH, FALSE,
                             &assembly_search_path, &end_assembly_search_path);
#endif /* ifdef CPPCLI_PORTABLE_ASSEMBLY_PATH */
#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES */
  /* LIBPATH is searched last.  Append it now because we have already added
     everything else to the assembly search path. */
  libpath = getenv("LIBPATH");
  if (libpath != NULL) {
    /* We will replace ';' with '\0' in the LIBPATH, so we must copy it. */
    libpath_size = strlen(libpath) + 1;
    current_path = alloc_general(libpath_size);
    strcpy(current_path, libpath);
    for (;;) {
      /* LIBPATH is a semicolon separated list of paths, so we must split
         on the ';'.  */
      semicolon = strchr(current_path, ';');
      if (semicolon != NULL) {
        /* We modify the string in place because we have a local copy. */
        *semicolon = '\0';
      }  /* if */
      add_to_specified_include_search_path(current_path, FALSE,
                            &assembly_search_path, &end_assembly_search_path);
      if (semicolon == NULL || (*(semicolon + 1) == '\0')) {
        /* If there wasn't a semicolon, or if there isn't anything after the
           semicolon, then we're done. */
        break;
      }  /* if */
      current_path = semicolon + 1;
    }  /* for */
  }  /* if */
}  /* init_assembly_search_path */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if READ_CPPCLI_PORTABLE_ASSEMBLIES || WRITE_CPPCLI_PORTABLE_ASSEMBLIES

void clear_portable_assembly_header(a_portable_assembly_header *header)
/*
Initialize the specified portable assembly header.
*/
{
  header->magic = 0;
  header->num_entries = 0;
  header->table_offset = 0;
}  /* clear_portable_assembly_header */

#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES || WRITE_CPPCLI_PORTABLE_ASSEMBLIES*/

#if MICROSOFT_EXTENSIONS_ALLOWED && (!CPPCLI_ENABLING_POSSIBLE || !EDG_WIN32)

#if READ_CPPCLI_PORTABLE_ASSEMBLIES
#if !USE_MMAP_FOR_MEMORY_REGIONS
#include <sys/mman.h>
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */

typedef struct a_portable_assembly_entry {
  /* This structure contains information about a portable assembly file
     whose C++/CLI metadata is being used in the current compilation. */
  a_const_char  *name;  /* The full name of the portable assembly. */
  FILE          *f_assembly;
                        /* A FILE pointer to the file. */
  void          *mmap_addr;
                        /* The address to which the file has been mapped to. */
  a_portable_assembly_header
                header;
                        /* The file header information for the portable
                           assembly. */
  a_portable_assembly_table_entry
                *table; /* A table of offsets (for each typedef) in the
                           portable assembly. */
} a_portable_assembly_entry;

STATIC_THREAD a_portable_assembly_entry
                *portable_assembly_table;
                        /* A dynamically-allocated table with information
                           about each portable assembly that is currently
                           available.  Note that entry zero in this table is
                           unused. */
STATIC_THREAD an_assembly_index
                pa_table_entries;
                        /* Contains the number of entries allocated for
                           portable_assembly_table. */
STATIC_THREAD an_assembly_index
                pa_cur_table_entry;
                        /* An index into portable_assembly_table that
                           represents the last entry in use (with the exception
                           that zero is not used). */


static void clear_portable_assembly_entry(a_portable_assembly_entry *entry)
/*
Initialize a portable assembly entry.
*/
{
  entry->name = NULL;
  entry->f_assembly = NULL;
  entry->mmap_addr = NULL;
  clear_portable_assembly_header(&entry->header);
  entry->table = NULL;
}  /* clear_portable_assembly_entry */

#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES */

/*
Substitute versions of metadata reading routines to aid in development on
platforms on which the metadata API is not available.  When
READ_CPPCLI_PORTABLE_ASSEMBLIES is TRUE, these substitute versions are
functional replacements for the corresponding routines in ms_metadata.cpp; when
READ_CPPCLI_PORTABLE_ASSEMBLIES is FALSE, the routines are simple stubs that
return an error indication.
*/

an_assembly_index import_metadata_file(
                      ARG_UNUSED a_const_char              *assembly_full_name,
                      ARG_UNUSED a_cpp_cli_import_flag_set import_flags,
                      ARG_UNUSED a_boolean                 *is_duplicate)
/*
Prepare an assembly for metadata import.  This is a substitute version
(the real function is in ms_metadata.cpp) that either returns an error or
attempts to read the specified portable assembly file that contains the
metadata obtained from the original assembly.  The latter configuration is
useful for testing on non-Windows platforms where metadata typically isn't
available.
*/
{
#if READ_CPPCLI_PORTABLE_ASSEMBLIES
  a_portable_assembly_entry *entry;
  FILE                      *file;
  an_assembly_index         idx = 0;
  struct stat               stat_buf;

  /* The full name of the "dll" (really a portable assembly) has been
     supplied. */
  check_assertion(assembly_full_name != NULL);
  /* See if we've opened this portable assembly before. */
  *is_duplicate = FALSE;
  if (portable_assembly_table != NULL) {
    for (an_assembly_index i = 1; i<=pa_cur_table_entry; i++) {
      if (strcmp(assembly_full_name, portable_assembly_table[i].name) == 0) {
        /* We've already opened this assembly, return its index along with
           an indication that the assembly has been previously imported. */
        idx = i;
        *is_duplicate = TRUE;
        goto end_of_routine;
      }  /* if */
    }  /* for */
  }  /* if */
  file = fopen_with_error(assembly_full_name, FOPEN_MODE_FOR_BINARY_READ,
                          OFF_NO_OPTIONS, ec_portable_assembly);
  if (file != NULL) {
    /* Note: an index of zero is used to indicate an error, so the first
       entry isn't used in the table. */
    idx = ++pa_cur_table_entry;
    if (idx >= pa_table_entries) {
      /* Dynamically allocate and grow the table as needed. */
      sizeof_t old_size = pa_table_entries * sizeof(a_portable_assembly_entry);
      pa_table_entries += 50;
      portable_assembly_table = (a_portable_assembly_entry *)realloc_buffer(
                               (char *)portable_assembly_table,
                               old_size,
                               pa_table_entries *
                                            sizeof(a_portable_assembly_entry));
    }  /* if */
    check_assertion(portable_assembly_table != NULL);
    entry = &portable_assembly_table[idx];
    clear_portable_assembly_entry(entry);
    if (fstat(fileno(file), &stat_buf) != 0) {
      goto close_file_with_error_return;
    }  /* if */
    /* Read the file header from the beginning of the file. */
    if (fscanf(file, PORTABLE_ASSEMBLY_HEADER_FORMAT,
                                           &entry->header.magic,
                                           &entry->header.num_entries,
                                           &entry->header.table_offset) != 3) {
      goto close_file_with_error_return;
    }  /* if */
    /* Check to make sure the data we're reading makes sense (i.e., that
       this really is a portable assembly file). */
    if (entry->header.magic != PORTABLE_ASSEMBLY_MAGIC_NUMBER ||
        stat_buf.st_size < (off_t)(entry->header.table_offset +
                                   entry->header.num_entries)) {
      goto close_file_with_error_return;
    }  /* if */
    entry->table = (a_portable_assembly_table_entry *)alloc_general(
                                      entry->header.num_entries *
                                      sizeof(a_portable_assembly_table_entry));
    /* Read in the table. */
    (void)fseek(file, entry->header.table_offset, SEEK_SET);
    for (an_assembly_index i = 0; i < entry->header.num_entries; i++) {
      if (fscanf(file, PORTABLE_ASSEMBLY_TABLE_FORMAT,
                                                 &entry->table[i].scope_index,
                                                 &entry->table[i].token,
                                                 &entry->table[i].offset,
                                                 &entry->table[i].size) != 4) {
        goto close_file_with_error_return;
      }  /* if */
    }  /* for */
    (void)fseek(file, 0L, SEEK_SET);
    entry->name = assembly_full_name;
    /* Map the file into our address space.  Note that the host-independent
       mmap routines aren't used here (they assume we're mapping to a known
       address), but that should be okay as it's unlikely that we'll use
       portable assemblies on a Windows platform.  We only map the amount
       necessary (the table isn't mapped). */
    entry->mmap_addr = mmap((caddr_t)0, entry->header.table_offset, PROT_READ,
                            MAP_PRIVATE, fileno(file), (off_t)0);
    if (entry->mmap_addr == MAP_FAILED) goto close_file_with_error_return;
    /* Save the file pointer so we can close it later. */
    entry->f_assembly = file;
    goto end_of_routine;
close_file_with_error_return:
    /* Return this entry in the table and close the file. */
    pa_cur_table_entry--;
    (void)fclose(file);
    idx = 0;
  }  /* if */
end_of_routine:
  return idx;
#else /* !READ_CPPCLI_PORTABLE_ASSEMBLIES */
  return 0;
#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES */
}  /* import_metadata_file */


void import_all_types(ARG_UNUSED an_assembly_index assembly_index,
                      ARG_UNUSED char              *buffer,
                      ARG_UNUSED size_t            *buffer_size)
/*
Import all types from the specified assembly into the buffer whose size
is in *buffer_size.  This routine is a substitute version for the actual
routine (in ms_metadata.cpp) and either does nothing or returns the desired
string from a portable assembly if so configured.
*/
{
#if READ_CPPCLI_PORTABLE_ASSEMBLIES
  a_boolean  is_delegate;

  /* The class declarations for the assembly are stored in the first entry
     (with a scope index of zero and a type-def token of zero): Return that
     entry. */
  import_class_definition(make_assembly_scope_index(assembly_index, 0), 0,
                          buffer, buffer_size, &is_delegate);
  check_assertion(!is_delegate);
#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES */
}  /* import_all_types */


void import_class_definition(
                    ARG_UNUSED an_assembly_scope_index assembly_scope_index,
                    ARG_UNUSED a_cpp_cli_token         metadata_type_def_token,
                    ARG_UNUSED char                    *buffer,
                    ARG_UNUSED size_t                  *buffer_size,
                    ARG_UNUSED a_boolean               *is_delegate)
/*
Import a specific class definition (as defined by metadata_type_def_token) from
the specified assembly into the buffer whose size is in *buffer_size.  This
routine is a substitute version for the actual routine (in ms_metadata.cpp) and
either does nothing or returns the desired string from a portable assembly if
so configured.
*/
{
#if READ_CPPCLI_PORTABLE_ASSEMBLIES
  a_portable_assembly_entry *entry;
  size_t                    size;
  unsigned int              i;
  an_assembly_index         assembly_index;
  a_scope_index             scope_index;
  char                      *class_def_str;

  assembly_index = assembly_index_from_assembly_scope_index(
                                                        assembly_scope_index);
  scope_index = scope_index_from_assembly_scope_index(assembly_scope_index);
  check_assertion(assembly_index <= pa_cur_table_entry);
  entry = &portable_assembly_table[assembly_index];
  for (i = 0; i < entry->header.num_entries; i++) {
    if (entry->table[i].scope_index == scope_index &&
        entry->table[i].token == metadata_type_def_token) {
      break;
    }  /* if */
  }  /* for */
  check_assertion(i < entry->header.num_entries);
  size = entry->table[i].size;
  class_def_str = (char *)entry->mmap_addr + entry->table[i].offset;
  if (size > sizeof(PORTABLE_ASSEMBLY_DELEGATE_PREFIX) &&
      strncmp(class_def_str, PORTABLE_ASSEMBLY_DELEGATE_PREFIX,
              sizeof(PORTABLE_ASSEMBLY_DELEGATE_PREFIX)) == 0) {
    /* The string starts with a prefix indicating that a delegate definition
       follows.  (That prefix isn't part of a C++/CLI definition, but is added
       when the portable assembly file is written.) */
    *is_delegate = TRUE;
    class_def_str += sizeof(PORTABLE_ASSEMBLY_DELEGATE_PREFIX);
    size -= sizeof(PORTABLE_ASSEMBLY_DELEGATE_PREFIX);
  } else {
    *is_delegate = FALSE;
  }  /* if */
  if (size > *buffer_size) {
    *buffer = '\0';
  } else {
    strncpy(buffer, class_def_str, size);
  }  /* if */
  *buffer_size = size;
#else /* !READ_CPPCLI_PORTABLE_ASSEMBLIES */
  *buffer_size = 0;
#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES */
}  /* import_class_definition */


void ms_metadata_trans_unit_init(ARG_UNUSED a_const_char *file_name) {}


void ms_metadata_trans_unit_wrapup(void)
/*
Reset the metadata reader for the next translation unit.  This clears all 
imported assemblies.
*/
{
#if READ_CPPCLI_PORTABLE_ASSEMBLIES
  /* Close everything, leaving the table allocated. */
  ms_metadata_cleanup();
#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES */
}  /* ms_metadata_trans_unit_wrapup */


void ms_metadata_cleanup(void)
/*
Cleanup as necessary.
*/
{
#if READ_CPPCLI_PORTABLE_ASSEMBLIES
  unsigned int i;
  /* Close any open files. */
  for (i = 1; i <= pa_cur_table_entry; i++) {
    a_portable_assembly_entry *entry = &portable_assembly_table[i];
    if (entry->mmap_addr != NULL) {
      (void)munmap((caddr_t)entry->mmap_addr, entry->header.table_offset);
    }  /* if */
    if (entry->f_assembly != NULL) {
      (void)fclose(entry->f_assembly);
    }  /* if */
    clear_portable_assembly_entry(entry);
  }  /* for */
  pa_cur_table_entry = 0;
#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES */
}  /* ms_metadata_cleanup */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED &&
          (!CPPCLI_ENABLING_POSSIBLE || !EDG_WIN32) */
#endif /* !STANDALONE_UTILITY_PROGRAM */

#if BUILTIN_FUNCTIONS_ENABLED

unsigned long strtoul_interface(a_const_char *str,
                                a_boolean    *err)
/*
An interface to strtoul that converts a non-NULL str to a numeric value and
returns it.  *err is set to TRUE if the string cannot be converted (and to
FALSE otherwise).
*/
{
  unsigned long result;
  char          *endptr;

  check_assertion(str != NULL);
  *err = FALSE;
  errno = 0;
  result = strtoul(str, &endptr, 0);
  if ((result == 0 || result == ULONG_MAX) &&
      (errno == EINVAL || errno == ERANGE)) {
    *err = TRUE;
  } else if (*str != '\0' && endptr != NULL && *endptr != '\0') {
    *err = TRUE;
  }  /* if */
  return result;
}  /* strtoul_interface */

#endif /* BUILTIN_FUNCTIONS_ENABLED */
#if !CUSTOM_DEFAULT_OUTPUT_FILES

FILE* default_error_output_file()
/*
This function returns the default FILE* that should be used for error output
(i.e., stderr).

This is the default implementation of the default_error_output_file function.
When CUSTOM_DEFAULT_OUTPUT_FILES is TRUE it's expected that an alternative
implementation is provided by another file (usually outside of the standard EDG
C++/C front end source file set).
*/
{
  return stderr;
}  /* default_error_output_file */


FILE* default_preproc_output_file()
/*
This function returns the default FILE* that should be used for preprocessor
output (i.e., stdout).

This is the default implementation of the default_preproc_output_file function.
When CUSTOM_DEFAULT_OUTPUT_FILES is TRUE it's expected that an alternative
implementation is provided by another file (usually outside of the standard EDG
C++/C front end source file set).
*/
{
  return stdout;
}  /* default_preproc_output_file */

#if NEED_IL_DISPLAY

FILE* default_il_display_output_file()
/*
This function returns the default FILE* that should be used for IL display
output (i.e., stdout).

This is the default implementation of the default_il_display_output_file
function.  When CUSTOM_DEFAULT_OUTPUT_FILES is TRUE it's expected that an
alternative implementation is provided by another file (usually outside of the
standard EDG C++/C front end source file set).
*/
{
  return stdout;
}  /* default_il_display_output_file */

#endif /* NEED_IL_DISPLAY */
#endif /* !CUSTOM_DEFAULT_OUTPUT_FILES */

void host_envir_one_time_init(void)
/*
Do one-time initialization related to host specific processing.  This
is done after command line processing.
*/
{
#if !STANDALONE_UTILITY_PROGRAM
  /* Note that these are translation unit variables, but they are not
     reinitialized for each translation unit.  They retain the value set
     for the primary translation unit, but are overwritten when loading
     exported template files. */
  register_trans_unit_variable(incl_search_path);
  register_trans_unit_variable(sys_incl_search_path);
  init_module_search_path();
#if MODULE_ID_NEEDED
  register_trans_unit_variable_with_field(module_id, module_id_ptr);
#endif /* MODULE_ID_NEEDED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled) {
    init_assembly_search_path();
  }  /* if */
#ifdef CPPCX_INCLUDE_PATH
  if (cppcx_enabled) {
    /* When using C++/CX, the vccorlib.h file is preincluded, and on
       non-Windows systems it won't be picked up in the default search path.
       This provides two hooks to add the directory where vccorlib.h can be
       found (specified at run time by the EDG_CPPCX_INCLUDE_PATH environment
       variable or at compile time by the CPPCX_INCLUDE_PATH configuration
       macro). */
    char *pa_path = getenv("EDG_CPPCX_INCLUDE_PATH");
    if (pa_path != NULL) {
      add_to_include_search_path(pa_path, /*system_include_dir=*/FALSE);
    }  /* if */
    add_to_include_search_path(CPPCX_INCLUDE_PATH,
                               /*system_include_dir=*/FALSE);
  }  /* if */
#endif /* ifdef CPPCX_INCLUDE_PATH */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* !STANDALONE_UTILITY_PROGRAM */
}  /* host_envir_one_time_init */


void host_envir_trans_unit_init(void)
/*
Initialize variables that are specific to a given translation unit.
*/
{
#if MODULE_ID_NEEDED
  module_id = NULL;
#endif /* MODULE_ID_NEEDED */
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
#if EDG_WIN32
  native_multibyte_locale = system_default_locale;
#endif /* EDG_WIN32 */
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
}  /* host_envir_trans_unit_init */


void host_envir_early_init(void)
/*
One time initialization that must take place early on in the front end.
This is done before command line processing.
*/
{
#if !MULTIPLE_THREAD_COMPILATION && !EDG_FRONT_END_DAEMON
  static a_boolean first_time = TRUE;

  if (first_time) {
    /* Set handlers for unusual abort signals.  This is only done on the
       first call, even if the front end can be restarted. */
    set_signal_handlers();
#if SVR4_TRAP_NULL_POINTER_REFERENCES
    svr4_trap_null_pointer_references();
#endif /* SVR4_TRAP_NULL_POINTER_REFERENCES */
    first_time = FALSE;
  }  /* if */
#endif /* !MULTIPLE_THREAD_COMPILATION && !EDG_FRONT_END_DAEMON */
  /* The temp_text_buffer is initialized here because it is used by
     get_curr_dir_name on some systems. */
  temp_text_buffer = NULL;
  size_temp_text_buffer = 0;
  pos_in_temp_text_buffer = 0;
  temp_text_buffer_managed = FALSE;
  dir_name_list_general = NULL;
  preinclude_file_list = NULL;
  macro_preinclude_file_list = NULL;
  preinclude_file_tail = NULL;
  macro_preinclude_file_tail = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  preusing_file_list = NULL;
  preusing_file_tail = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  template_search_path = NULL;
  template_search_path_tail = NULL;
  avail_directory_name_entries = NULL;
  C_dialect = C_dialect_cplusplus;
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
  locale_already_set = FALSE;
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
#if !STANDALONE_UTILITY_PROGRAM
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
#if EDG_WIN32
  /* Create a locale object to be used for multibyte character conversions. */
  system_default_locale = _create_locale(LC_ALL,
                                         get_system_default_locale_name());
  check_assertion(system_default_locale != NULL);
#endif /* EDG_WIN32 */
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
#endif /* !STANDALONE_UTILITY_PROGRAM */
  /* The floating-point routines require that "." be the radix point, so
     ensure that LC_NUMERIC is set to "C" (all programs start off with this
     as the default, but the locale may have been changed by user code). */
  if (setlocale(LC_NUMERIC, "C") == NULL) {
    catastrophe(ec_setlocale_lc_numeric_failed);
  }  /* if */
  current_directory_name = NULL;
  /* Get the name of the EDG_BASE directory.  This may be overridden by
     a command-line option.  If the environment variable is not set, use
     a built-time default value. */
  edg_base_directory = getenv("EDG_BASE");
#if CHECKING
  /* Look for an environment variable named EDG_SUPPRESS_ASSERTION_LINE_NUMBER.
     If it is set to value other than zero, set a flag indicating that the
     line number portion of an "assertion failed" message should be
     suppressed. */
  {
    char	*str;
    suppress_assertion_line_number = FALSE;
    str = getenv("EDG_SUPPRESS_ASSERTION_LINE_NUMBER");
    if (str != NULL && strcmp(str, "0") != 0) {
      suppress_assertion_line_number = TRUE;
    }  /* if */
  }
#endif /* CHECKING */
  if (edg_base_directory == NULL) edg_base_directory = DEFAULT_EDG_BASE;
  /* Determine whether the host system is big or little endian. */
  /* Suppress the CodeCenter warning that would be issued because we
     access an "int" using a "char" pointer. */
  /*SUPPRESS 112 */
  { int		i = 1;
    host_little_endian = (*(char *)&i) == 1;
  }
  file_read_buffer = NULL;
  dir_and_file_buffer = NULL;
  format_file_name_buffer = NULL;
#if EDG_WIN32 && NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
  locale_name_buffer = NULL;
#endif /* EDG_WIN32 && NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
  utf8_buffer = NULL;
  mbc_buffer = NULL;
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
#if EDG_WIN32 && UNICODE_SOURCE_SUPPORTED
  wchar_translation_buffer = NULL;
#endif /* EDG_WIN32 && UNICODE_SOURCE_SUPPORTED */
#if !STANDALONE_UTILITY_PROGRAM
#if EDG_WIN32
  conv_utf8_buffer = NULL;
#else /* !EDG_WIN32 */
#if MICROSOFT_EXTENSIONS_ALLOWED
  default_cpp_cli_import_flags = (int)cpp_cli_none;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* EDG_WIN32 */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#if __MICROSOFT_OS__
  open_temp_files = NULL;
#endif /* __MICROSOFT_OS__ */
  temp_dir = NULL;
  temp_dir_len = 0;
#if !USE_HOST_TMPFILE_FACILITIES
  temp_seed = 0;
#endif /* !USE_HOST_TMPFILE_FACILITIES */
#if MODULE_ID_NEEDED
  module_id = NULL;
#endif /* MODULE_ID_NEEDED */
  dir_buffer1 = NULL;
  dir_buffer2 = NULL;
  primary_source_file_name = NULL;
  dir_name_of_primary_source_file = NULL;
#if COMPILE_MULTIPLE_SOURCE_FILES
  more_than_one_source_file = FALSE;
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */
  more_than_one_non_export_translation_unit = FALSE;
  object_file_name = NULL;
  /* Start with empty include file search paths.  Entries may be added
     because of command line options, and others will be added as defaults. */
  incl_search_path = NULL;
  end_incl_search_path = NULL;
  embed_search_path = NULL;
  end_embed_search_path = NULL;
  sys_incl_search_path = NULL;
  put_dir_of_each_opened_source_file_on_incl_search_path = TRUE;
  stack_referenced_include_directories = STACK_REFERENCED_INCLUDE_DIRECTORIES;
  backslash_is_also_dir_separator = BACKSLASH_IS_ALSO_DIR_SEPARATOR;
  windows_paths_allowed = WINDOWS_PATHS_ALLOWED;
  module_search_path = NULL;
  end_module_search_path = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  assembly_search_path = NULL;
  end_assembly_search_path = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  prototype_instantiations_in_il = PROTOTYPE_INSTANTIATIONS_IN_IL;
#if DO_IL_LOWERING
   all_template_info_in_il = prototype_instantiations_in_il;
#else /* !DO_IL_LOWERING */
  all_template_info_in_il = ALL_TEMPLATE_INFO_IN_IL;
#endif /* DO_IL_LOWERING */
  in_front_end = FALSE;
  pragma_define_type_info_is_required = PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED;
  use_predefined_macro_file = DEFAULT_USE_PREDEFINED_MACRO_FILE;
  memzero((a_void_ptr)predef_macro_mode_values,
	  sizeof(predef_macro_mode_values));
#if UNICODE_SOURCE_SUPPORTED
  check_for_byte_order_mark = DEFAULT_CHECK_FOR_BYTE_ORDER_MARK;
  default_unicode_source_kind = DEFAULT_UNICODE_SOURCE_KIND;
#endif /* UNICODE_SOURCE_SUPPORTED */
#if MAKE_FRONT_END_CALLABLE
  exit_status = 0;
#endif /* MAKE_FRONT_END_CALLABLE */
#if USE_MMAP_FOR_MEMORY_REGIONS
  page_size = 0;
#if !EDG_WIN32 && MMAP_FIXED_NOREPLACE_FLAG
  try_mmap_fixed_noreplace = TRUE;
#endif /* !EDG_WIN32 && MMAP_FIXED_NOREPLACE_FLAG */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
#if !EDG_WIN32 && !MULTIPLE_THREAD_COMPILATION
  reset_cpu_time_limit();
#endif /* !EDG_WIN32 && !MULTIPLE_THREAD_COMPILATION */
#if MICROSOFT_EXTENSIONS_ALLOWED && (!CPPCLI_ENABLING_POSSIBLE || !EDG_WIN32)
#if READ_CPPCLI_PORTABLE_ASSEMBLIES && !STANDALONE_UTILITY_PROGRAM
  portable_assembly_table = NULL;
  pa_table_entries = 0;
  pa_cur_table_entry = 0;
#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES && !STANDALONE_UTILITY_PROGRAM */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED &&
          (!CPPCLI_ENABLING_POSSIBLE || !EDG_WIN32) */
  add_match_notes = DEFAULT_ADD_MATCH_NOTES;
  /* If the host environment and the front end configuration did not provide a
     definition for UINT32_MAX or UINT64_MAX, we defaulted that macro to
     UINT_MAX.  Check that this does not exceed the capacity of the uint32_t or
     uint64_t type respectively. */
  static_assert(sizeof(UINT32_MAX) <= sizeof(uint32_t),
                "UINT32_MAX is too large");
  static_assert(sizeof(UINT64_MAX) <= sizeof(uint64_t),
                "UINT64_MAX is too large");
}  /* host_envir_early_init */


void host_envir_init(void)
/*
Initialize static variables related to the host specific routines. 
This is done as a subroutine (rather than relying on static initialization)
so that it can be redone to compile more than one source file in a single
invocation of the front end.
*/
{
  dir_name_list_il = NULL;
}  /* host_envir_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/*
The host_util.h file is used to define functions that are used by both
the front end, and utility programs such as the prelinker.  Include the
file here to define these functions for the front end.
*/

#include "host_util.h"


