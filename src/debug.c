/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

debug.c -- Debug routines.

*/

/* Header files common to all files. */
#include "basic_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* The #if here is done after the inclusion of basics.h because basics.h
   may set DEBUG. */
#if DEBUG
/* GCOVR_EXCL_START */

#include "fe_common.h"

#ifndef STDLIB_H_INCLUDED
extern "C" int atoi(char *);
#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
extern "C" long atol(char *);
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */
#endif /* ifndef STDLIB_H_INCLUDED */

/*
The stop token checksum test is only done when CHECKING code is included
and not in a standalone utility program.
*/
#if CHECKING && !STANDALONE_UTILITY_PROGRAM
#define STOP_TOKEN_CHECKSUM_TEST_NEEDED TRUE
#else /* !(CHECKING && !STANDALONE_UTILITY_PROGRAM) */
#define STOP_TOKEN_CHECKSUM_TEST_NEEDED FALSE
#endif /* CHECKING && !STANDALONE_UTILITY_PROGRAM */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
The structure defining the linked list of routines from which debug information
has been requested.
*/
enum a_debug_action {
  da_none,
  da_set_level,
  da_increase_level,
  da_decrease_level,
  da_set_flag,
#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
  da_alloc_seq,
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */
  da_name
};

/*
A command line debug request.
*/
typedef struct a_debug_request *a_debug_request_ptr;
typedef struct a_debug_request {
  a_debug_request_ptr
		next;
  char          *name;
			/* Pointer to name of function to debug. */
  a_debug_action
		action;
		        /* What to do on entry and exit from this routine. */
  int           level;
		        /* The absolute or relative debug level to set. */
  a_boolean     do_not_print_message;
			/* If TRUE, do not print the entry and exit
			   messages. */
#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
  unsigned long	alloc_seq_number;
			/* An allocation sequence number to watch for. */
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */
} a_debug_request;

/*
The list of debug requests.
*/
STATIC_THREAD a_debug_request_ptr
		debug_requests;

/*
Stack of function calls.  Each time db_enter is invoked, another entry is
added to the stack; db_exit removes it.
*/
typedef struct a_debug_stack_entry *a_debug_stack_entry_ptr;
typedef struct a_debug_stack_entry {
  a_const_char	*name;
	        	/* The name of the function. */
  int           old_debug_level;
	                /* The value of the debug level before it was
			   changed on entry to this function. */
  a_boolean     msg_was_printed;
	                /* Was a message printed on entry to this function? */
#if STOP_TOKEN_CHECKSUM_TEST_NEEDED
  unsigned	stop_token_checksum;
			/* Checksum of the stop token array, stored on entry
			   and checked on exit, to catch cases where the stop
			   tokens are not being correctly maintained. */
#endif /* STOP_TOKEN_CHECKSUM_TEST_NEEDED */
} a_debug_stack_entry;

#define DEBUG_STACK_INITIAL_ALLOCATION 128
			/* The initial allocated size of the debug stack. */

STATIC_THREAD a_debug_stack_entry_ptr
		debug_stack;
			/* Pointer to the dynamically allocated debug stack. */

STATIC_THREAD int
		depth_debug_stack;
			/* The current depth of the debug stack. */

STATIC_THREAD sizeof_t
		debug_stack_size = 0;
			/* The allocated size of the debug stack. */


static a_debug_request_ptr alloc_debug_request(void)
/*
Allocate and initialize a debug request record.
*/
{
  a_debug_request_ptr ptr;

  ptr = (a_debug_request_ptr)alloc_general(sizeof(a_debug_request));
  ptr->next   = NULL;
  ptr->name   = NULL;
  ptr->action = da_set_level;
  ptr->level  = 0;
  ptr->do_not_print_message = FALSE;
#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
  ptr->alloc_seq_number = 0;
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */

  return(ptr);
}  /* alloc_debug_request */


static void remove_debug_flag(char *function_name)
                                           
/*
If the debug request list contains a da_set_flag entry for the specified name,
remove the entry.
*/
{
  a_debug_request_ptr   request_ptr;
  a_debug_request_ptr   prev_request_ptr = NULL;
  
  /* Run through the list of debug requests and see if this name appears. */
  for (request_ptr = debug_requests; request_ptr != NULL;
       prev_request_ptr = request_ptr, request_ptr = request_ptr->next) {
    if (request_ptr->action == da_set_flag &&
        strcmp(function_name, request_ptr->name) == 0) {
      break;
    }  /* if */
  }  /* for */
  if (request_ptr != NULL) {
    /* An entry was found.  Remove it from the list. */
    if (prev_request_ptr == NULL) {
      /* It must be the first entry on the list. */
      debug_requests = debug_requests->next;
    } else {
      /* It is not the first entry on the list. */
      prev_request_ptr->next = request_ptr->next;
    }  /* if */
  }  /* if */
}  /* remove_debug_flag */


a_boolean proc_debug_option(a_const_char *debug_option)
/*
Parse the debug option (as received by proc_command_line) and either set
the global debug variable to some static value, or build a data structure
indicating changes to the global debug value on entry and exit to specific
routines.  The option is expected to have one of the following
forms:

  -dnumber[!]
  -dname=number[!]
  -dname+=number[!]
  -dname-=number[!]
  -d-name

  =   set the debug level to the value following the equal sign, always print
      the entry and exit message
  +=  increment the debug level by the value following the equal sign, always
      print the entry and exit message
  -=  decrement the debug level by the value following the equal sign, always
      print the entry and exit message

      If the number following any of the above is followed by a !, do not
      print the entry end exit message, but still perform the action specified.

Options of the form "-d-name" are used to set the debug flag named "name" to
TRUE.  When multiple "set flag" options are specified, they must be specified
using the format

	-d-flag1,-flag2

Options of the form "-d#name" cause the debug flag named "name" to be removed
from the list of flags.

Returns TRUE if there was an error during parsing of the debug option.
*/
{
  a_const_char         *curr_char;
  char                 curr_name[128];
  char                 *curr_name_ptr;
  a_debug_request_ptr  head;
  a_debug_request_ptr  request;
  int                  level;
  a_debug_action       action;
  a_boolean            do_not_print_message;
  a_boolean            dump_list = FALSE;
  a_boolean            done;
  a_boolean            remove_flag = FALSE;

  db_active = TRUE;
  if (isdigit((unsigned char)*debug_option)) {
    /* The option is just a number, set the global debug level. */
    debug_level = atoi(debug_option);
  } else {
    /* The option consists of requests to alter the debug level based on entry
       and exit to specific routines. */
    curr_char = debug_option;
    do {

      head = NULL;
      action = da_none;
      do_not_print_message = FALSE;
      level = 0;
      do {

        if (*curr_char == '-') {
          /* Set the specified debug flag to TRUE. */
          action = da_set_flag;
          curr_char++;
          if (*curr_char == ' ') curr_char++;
        } else if (*curr_char == '#') {
          /* Clear the specified debug flag. */
          action = da_set_flag;
          remove_flag = TRUE;
          curr_char++;
          if (*curr_char == ' ') curr_char++;
        } else {
          /* The first thing must be the name of the routine. */
          if (!isalpha((unsigned char)*curr_char)) {
            goto error_exit;
          }  /* if */
        }  /* if */

        /* Gather up the name of the routine. */
        curr_name_ptr = curr_name;
        while (isalnum((unsigned char)*curr_char) || (*curr_char == '_')) {
	  *curr_name_ptr++ = *curr_char++;
        }  /* while */
        *curr_name_ptr = '\0';
	if (strcmp(curr_name, "proc_debug_option") == 0) {
	  dump_list = TRUE;
	}  /* if */

        if (!remove_flag) {
          /* Allocate a record to contain the request. */
          request = alloc_debug_request();
          request->name = alloc_general((sizeof_t)(strlen(curr_name) + 1));
          (void)strcpy(request->name, curr_name);

          /* Add the record to the local list. */
          request->next = head;
          head = request;
        } else {
          /* Remove the named flag from the global list. */
          remove_debug_flag(curr_name);
        }  /* if */

        /* If the next character is a comma, then another name follows.  If
	   not, then the character must be either an equals, a plus, or a
	   minus.  "set flag" options may not be in a comma separated
           list. */
        done = (*curr_char != ',' || action != da_none);
        if (!done) curr_char++;
      } while (!done);
      if (action == da_set_flag) {
        /* There are no arguments following a set flag option. */
      } else {
        switch (*curr_char++) {
          case '=':
            action = da_set_level;
            break;
         case '+':
           if (*curr_char++ != '=') goto error_exit;
           action = da_increase_level;
           break;
         case '-':
           if (*curr_char++ != '=') goto error_exit;
           action = da_decrease_level;
           break;
         default:
           goto error_exit;
         }  /* switch */

        /* There should be a number following the action. */
        level = 0;
        if (!isdigit((unsigned char)*curr_char)) {
          goto error_exit;
        }  /* if */
        while (isdigit((unsigned char)*curr_char)) {
          level = (level * 10) + (*curr_char++ - '0');
        }  /* while */
        /* "!" at the end indicates that the entry/exit message should not
           be printed. */
        if (*curr_char == '!') {
          do_not_print_message = TRUE;
          curr_char++;
        } else {
         do_not_print_message = FALSE;
        }  /* if */
      }  /* if */

      /* Now update all of the requests in the local list and move them to the
	 global list. */
      request = head;
      while (request != NULL) {
	request->action = action;
	request->do_not_print_message = do_not_print_message;
	request->level = level;
	if (request->next == NULL) {
	  /* Last record in the list, link it to the first record on the global
	     list, make the head of the global list the head of this list. */
	  request->next = debug_requests;
	  debug_requests = head;
	  request = NULL;
	} else {
	  request = request->next;
	}  /* if */
      }  /* while */
      done = (*curr_char != ',');
      if (!done) curr_char++;
    } while(!done);
  }  /* if */
#if DEBUG
  /* Special debug case.  If there are any requests on the list, dump them all
     if the name of this routine appears. */
  if ((debug_requests != NULL) && dump_list) {
    request = debug_requests;
    while (request != NULL) {
      fprintf(f_debug, "debug request for: %s\n", request->name);
      fprintf(f_debug, "action=%d,  level=%d\n", (int)request->action,
	      request->level);
      request = request->next;
    }  /* while */
  }  /* if */
#endif /* DEBUG */

/* Normal exit. */
  return(FALSE);

error_exit:
  return(TRUE);
}  /* proc_debug_option */


a_boolean debug_flag_is_set(a_const_char *function_name)
/*
Return TRUE if the debug request list contains a da_set_flag entry
for the specified name. 
*/
{
  a_debug_request_ptr   request_ptr;
  a_boolean		result = FALSE;

  /* Run through the list of debug requests and see if this name appears. */
  request_ptr = debug_requests;
  while (request_ptr != NULL) {
    if (request_ptr->action == da_set_flag &&
        strcmp(function_name, request_ptr->name) == 0) {
      result = TRUE;
      break;
    }  /* if */
    request_ptr = request_ptr->next;
  }  /* while */
  return result;
}  /* debug_flag_is_set */


a_boolean f_db_has_traced_name(a_source_correspondence *scp,
                               an_il_entry_kind        entry_kind)
/*
Return TRUE if the indicated source correspondence has a name that
is to be traced.  entry_kind indicates the IL entry kind.  If
entry_kind indicates an entity that does not have a source
correspondence, do nothing.  However, alloc-seq debug requests
are recognized for all entry kinds.
*/
{
  a_boolean           result = FALSE;
  a_debug_request_ptr request;

  if (debug_requests != NULL) {
    /* Only do this for named declarative entries. */
    if (source_corresp_for_il_entry((char *)scp, entry_kind) != NULL &&
        scp->name != NULL) {
      char *name = NULL;
      uint32_t len_of_name_without_params = 0;
      /* Compare it against the list of debug requests. */
      for (request = debug_requests;
           request != NULL;
           request = request->next) {
        if (request->action == da_name) {
          char      *eff_name;
          char      *eff_request_name = request->name;
          char      name_char_to_restore = '\0';
          a_boolean restore_char = FALSE;
          if (name == NULL) {
            /* Get the entity name.  Do it lazily in case the only debug
               requests are for allocation sequence number checks. */
            name = db_name_str_full(scp, entry_kind,
                                    /*include_func_params=*/FALSE);
            if (entry_kind == (an_il_entry_kind)iek_routine) {
              len_of_name_without_params = (uint32_t)strlen(name);
              /* Generate a version with parameter types in case it's
                 needed. */
              name = db_name_str_full(scp, entry_kind,
                                      /*include_func_params=*/TRUE);
            }  /* if */
          }  /* if */
          eff_name = name;
          if (len_of_name_without_params != 0 &&
              strchr(eff_request_name, '(') == NULL) {
            /* The request has no left parenthesis, so compare against the
               form of the name without parameter types.  Truncate the
               name temporarily by inserting a null character at the right
               place. */
            name_char_to_restore = name[len_of_name_without_params];
            name[len_of_name_without_params] = '\0';
            restore_char = TRUE;
          }  /* if */
          if (eff_request_name[0] != '[') {
            /* A name on the command line without a translation unit file
               name matches any translation unit.  Skip past the translation
               unit name on the generated name, if there is one. */
            if (eff_name[0] == '[') {
              check_assertion(eff_name != NULL);
              eff_name = strchr(eff_name, ']');
              check_assertion(eff_name != NULL);
              eff_name++;
            }  /* if */
          } else if (eff_request_name[1] == ']') {
            /* A name on the command line beginning with [] matches only
               a name in the primary translation unit. */
            if (eff_name[0] == '[') continue;
            eff_request_name += 2;
          }  /* if */
          check_assertion(eff_name != NULL);
          if (strcmp(eff_name, eff_request_name) == 0) {
            /* A match. */
            result = TRUE;
            break;
          }  /* if */
          if (restore_char) {
            name[len_of_name_without_params] = name_char_to_restore;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
    if (!result) {
      /* Look for alloc-seq requests. */
      for (request = debug_requests;
           request != NULL;
           request = request->next) {
        if (request->action == da_alloc_seq) {
          if (il_entry_prefix_of(scp).alloc_seq_number ==
                                                   request->alloc_seq_number) {
            result = TRUE;
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */
  }  /* if */
  return result;
}  /* f_db_has_traced_name */

#if !STANDALONE_UTILITY_PROGRAM

a_boolean f_db_sym_has_traced_name(a_symbol_ptr	sym)
/*
Return TRUE if the IL entry associated with the symbol "sym" has a name
that is to be traced.
*/
{
  char			*entry;
  an_il_entry_kind	kind;
  a_boolean		result = FALSE;

  if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
    /* For an overload set, return TRUE if any member has a traced name. */
    for (sym = sym->variant.overloaded_function.symbols;
         sym != NULL; sym = sym->next) {
      result = f_db_sym_has_traced_name(sym);
      if (result) break;
    }  /* for */
  } else {
    entry = il_entry_for_symbol_null_okay(sym, &kind);
    if (entry != NULL) {
      result = db_has_traced_name(entry, kind);
    }  /* if */
  }  /* if */
  return result;
}  /* f_db_sym_has_traced_name */


a_boolean f_db_sym_trace(a_const_char	*flag_name,
			 a_symbol_ptr	sym)
/*
Return TRUE if either (a) there are no debug flags set, and
f_db_has_traced_name is TRUE for the IL entry associated with sym,
or (b) the debug flag with the name "flag_name" is set, and
either there are no debug name requests in the request list or
f_db_has_traced_name is TRUE for the IL entry associated with sym.  Use
the macro db_sym_trace to call this function.
*/
{
  char			*entry;
  an_il_entry_kind	kind;
  a_boolean		result = FALSE;

  if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
    /* For an overload set, return TRUE if any member has a traced name. */
    for (sym = sym->variant.overloaded_function.symbols;
         sym != NULL; sym = sym->next) {
      result = f_db_sym_trace(flag_name, sym);
      if (result) break;
    }  /* for */
  } else {
    entry = il_entry_for_symbol_null_okay(sym, &kind);
    if (entry != NULL) {
      result = db_trace(flag_name, entry, kind);
    }  /* if */
  }  /* if */
  return result;
}  /* f_db_sym_trace */


#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean f_db_trace(a_const_char     *flag_name,
                     char             *entry,
                     an_il_entry_kind kind)
/*
Return TRUE if either (a) there are no debug flags set, and
f_db_has_traced_name is TRUE for the indicated entry/kind, or
(b) the debug flag with the name "flag_name" is set, and
either there are no debug name requests in the request list or
f_db_has_traced_name is TRUE for the indicated entry/kind.  Use
the macro db_trace to call this function.
*/
{
  a_boolean           result = FALSE;
  a_debug_request_ptr request;
  a_boolean           any_name_requests = FALSE;
  a_boolean           any_flag_requests = FALSE;

  /* See if there are any debug-name, alloc-seq, or flag requests. */
  for (request = debug_requests; request != NULL; request = request->next) {
    if (request->action == da_name
#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
        || request->action == da_alloc_seq
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */
                                          ) {
      any_name_requests = TRUE;
      if (any_flag_requests) break;
    } else if (request->action == da_set_flag) {
      any_flag_requests = TRUE;
      if (any_name_requests) break;
    }  /* if */
  }  /* for */
  if (!any_flag_requests) {
    /* There are no set flags, so produce output if the name is being
       traced. */
    result = f_db_has_traced_name((a_source_correspondence *)entry, kind);
  } else if (debug_flag_is_set(flag_name)) {
    if (!any_name_requests) {
      /* No name requests, so produce output for all cases. */
      result = TRUE;
    } else {
      /* Some name requests, so produce output only for the indicated
         names. */
      result = f_db_has_traced_name((a_source_correspondence *)entry, kind);
    }  /* if */
  }  /* if */
  return result;
}  /* f_db_trace */


a_boolean proc_debug_name_option(a_const_char *debug_option)
/*
Parse the debug_name option (as received by proc_command_line) and enter
information about it in the debug requests list.  Its format is

  --db_name=name

Return TRUE if there was an error.  If the name includes a translation
unit name, e.g., --db_name=[test_2.c]A::X, only the name in that (secondary)
translation unit will match.  If the name does not include a translation
unit name, e.g., --db_name==A::X, a name from any translation unit will match.
To select only a name from the primary translation unit, use [], e.g.,
--db_name=[]A::X.
*/
{
  a_debug_request_ptr request;

  db_active = TRUE;
  request = alloc_debug_request();
  request->action = da_name;
  request->name = alloc_general((sizeof_t)(strlen(debug_option) + 1));
  (void)strcpy(request->name, debug_option);
  request->next = debug_requests;
  debug_requests = request;
  return FALSE;
}  /* proc_debug_name_option */

#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER

a_boolean proc_debug_alloc_seq_option(a_const_char *debug_option)
/*
Parse the db_alloc_seq option (as received by proc_command_line) and enter
information about it in the debug requests list.  Its format is

  --db_alloc_seq=number

Return TRUE if there was an error.
*/
{
  a_debug_request_ptr request;

  db_active = TRUE;
  request = alloc_debug_request();
  request->action = da_alloc_seq;
  request->alloc_seq_number = atol(debug_option);
  request->next = debug_requests;
  debug_requests = request;
  return FALSE;
}  /* proc_debug_alloc_seq_option */

#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */

void debug_enter(int reporting_level, a_const_char *function_name)
/*
Place the name of this function on the stack.  If the name of this function
appears in the debug request list, do what the request indicates and remember
what was done in the stack entry.
*/
{
  a_debug_request_ptr request_ptr;
  a_debug_stack_entry *stack_ptr;

  if (depth_debug_stack >= ((int)debug_stack_size) - 1) {
    sizeof_t	new_size;
    if (debug_stack_size == 0) {
      new_size = DEBUG_STACK_INITIAL_ALLOCATION;
    } else {
      /* Double the size of the stack. */
      new_size = debug_stack_size * 2;
    }  /* if */
    debug_stack = (a_debug_stack_entry_ptr)realloc_buffer(
                      (char *)debug_stack,
                      (sizeof_t)(debug_stack_size*sizeof(a_debug_stack_entry)),
                      (sizeof_t)(new_size*sizeof(a_debug_stack_entry)));
    debug_stack_size = new_size;
  }  /* if */

  /* Get a new stack entry and increment the stack level. */
  stack_ptr = &debug_stack[++depth_debug_stack];
  stack_ptr->name = function_name;
  /* Remember the current debug level in case it changes. */
  stack_ptr->old_debug_level = debug_level;
#if STOP_TOKEN_CHECKSUM_TEST_NEEDED
  /* Store the checksum of the stop token array, for checking at exit.
     The values in the stop token array are supposed to be the same on
     exit from a routine as they were on entry. */
  stack_ptr->stop_token_checksum = 0;
  if (debug_level > 0 && curr_stop_token_stack_entry != NULL) {
    int				i;
    a_token_set_array_element	*stop_token_ptr;
    stop_token_ptr = curr_stop_token_stack_entry->stop_tokens;
    for (i = 0; i <= (int)tok_last; i++) {
      stack_ptr->stop_token_checksum += *stop_token_ptr++;
    }  /* for */
  }  /* if */
#endif /* STOP_TOKEN_CHECKSUM_TEST_NEEDED */

  /* Run through the list of debug requests and see if this function
     appears. */
  for (request_ptr = debug_requests;
       request_ptr != NULL; request_ptr = request_ptr->next) {
    if ((request_ptr->action == da_set_level ||
         request_ptr->action == da_increase_level ||
         request_ptr->action == da_decrease_level) &&
        strcmp(function_name, request_ptr->name) == 0) {
      /* What to do to the debug level. */
      switch (request_ptr->action) {
	case da_set_level:
	  debug_level = request_ptr->level;
	  break;
	case da_increase_level:
	  debug_level += request_ptr->level;
	  break;
	case da_decrease_level:
	  debug_level -= request_ptr->level;
	  break;
        default:
          unexpected_condition();
      }  /* switch */
      /* Found a match, break out of while loop. */
      break;
    }  /* if */
  }  /* for */
  stack_ptr->msg_was_printed = FALSE;
  if (request_ptr != NULL) {
    if (!request_ptr->do_not_print_message) {
      stack_ptr->msg_was_printed = TRUE;
      fprintf(f_debug, "==> %s (debug level changed from %d to %d)\n",
              function_name, stack_ptr->old_debug_level, debug_level);
      (void)fflush(f_debug);
    }  /* if */
  } else if (debug_level >= reporting_level) {
    stack_ptr->msg_was_printed = TRUE;
    fprintf(f_debug, "==> %s\n", function_name);
    (void)fflush(f_debug);
  }  /* if */
}  /* debug_enter */


void debug_exit(void)
/*
Print an exiting message if the current stack entry indicates that a message
was printed on entry.  Remove the entry from the stack.
*/
{
  a_debug_stack_entry *stack_ptr;

#if CHECKING
  if (depth_debug_stack < 0) {
    /* We have run off the beginning of the stack; abort. */
    internal_error("debug_exit: stack underflow");
  }  /* if */
#endif /* CHECKING */

  stack_ptr = &debug_stack[depth_debug_stack--];

  /* Check if a message was printed on entry to this function. */
  if (stack_ptr->msg_was_printed) {
    /* If the debug level changed on entry, print a level changed message. */
    if (stack_ptr->old_debug_level != debug_level) {
      fprintf(f_debug, "<== %s (debug level changed from %d to %d)\n",
	      stack_ptr->name, debug_level,
	      stack_ptr->old_debug_level);
    } else {
      fprintf(f_debug, "<== %s\n", stack_ptr->name);
    }  /* if */
    (void)fflush(f_debug);
  }  /* if */
  /* Restore debug level in case it was changed. */
  debug_level = stack_ptr->old_debug_level;
#if STOP_TOKEN_CHECKSUM_TEST_NEEDED
  /* Check the stop token array checksum if one was computed on entry. */
  if (debug_level > 0 && curr_stop_token_stack_entry != NULL) {
    int      i;
    unsigned test_checksum = 0;
    a_token_set_array_element
             *stop_token_ptr = curr_stop_token_stack_entry->stop_tokens;
    for (i = 0; i <= (int)tok_last; i++) {
      test_checksum += *stop_token_ptr++;
    }  /* for */
    if (test_checksum != stack_ptr->stop_token_checksum) {
      fprintf(f_debug,
                  "Stop tokens set checksum incorrect at exit from \"%s\".\n",
                  stack_ptr->name);
      internal_error("debug_exit: stop tokens set checksum is incorrect");
    }  /* if */
  }  /* if */
#endif /* STOP_TOKEN_CHECKSUM_TEST_NEEDED */
}  /* debug_exit */


void db_print_indent(unsigned amount)
/*
Print the given amount of double space indent.
*/
{
  for (unsigned i = 0; i < amount; ++i) {
    fprintf(f_debug, "  ");
  }  /* for */
}  /* db_print_indent */


void debug_early_init(void)
/*
One time initialization that must take place early on in the front end.
This routine is called before host_envir_early_init is called, so it
must not make use of anything initialized there.
*/
{
  debug_requests = NULL;
  debug_stack = NULL;
  depth_debug_stack = -1;
  debug_stack_size = 0;
  /* Make sure the db_active flag is reset. */
  db_active = FALSE;
  debug_level = 0;
}  /* debug_early_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */


