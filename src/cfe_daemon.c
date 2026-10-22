/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

cfe_daemon.c -- Unix socket based server for the C++/C front end daemon.

This file is provided as an example of using the front end as a library to
build a multi-threaded client/server C++/C compiler front end.

This daemon and its corresponding client are not (currently) production grade
and are intended for exposition only.

See cfe_daemon_common.h for documentation on the daemon protocol.
*/

/* Header files common to all files. */
#include "fe_common.h"

/* See if this code is needed at all. */
#if EDG_FRONT_END_DAEMON

/* Verify the configuration is compatible with the multi-threaded driver
   code. */
#if !MAKE_FRONT_END_CALLABLE
 #error -- The mutli-threaded C++/C-daemon requires MAKE_FRONT_END_CALLABLE
#endif /* !MAKE_FRONT_END_CALLABLE */
#if !CUSTOM_DEFAULT_OUTPUT_FILES
 #error -- The multi-threaded C++/C-daemon requires CUSTOM_DEFAULT_OUTPUT_FILES
#endif /* !CUSTOM_DEFAULT_OUTPUT_FILES */

/* Common daemon code. */
#include "cfe_daemon_common.h"

/* Memory management. */
#include "direct_allocator.h"

/* Unix headers used to implement the daemon. */
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

namespace {

/*
This struct represents the arguments required to start a new front end thread.
*/
struct an_invocation_ticket {
  int           communication_fd;
                        /* The file descriptor identifier used to communicate
                           with the client associated with this thread. */
  ~an_invocation_ticket();
};  /* an_invocation_ticket */


an_invocation_ticket::~an_invocation_ticket()
/*
Deconstruct the invocation ticket.
*/
{
  if (this->communication_fd != -1) {
    close(this->communication_fd);
  }  /* if */
}  /* an_invocation_ticket::~an_invocation_ticket */

}  /* namespace */

static pthread_cond_t
                any_work;
                        /* This condition is awaited upon for threads looking
                           for work (guarded by the next ticket lock). */
static an_invocation_ticket
                *next_ticket;
                        /* The next ticket to work on (guarded by the next
                           ticket lock). */
static pthread_mutex_t
                next_ticket_lock;
                        /* The work lock is acquired to update the any_work
                           condition and next ticket value. */

using an_owned_invocation_ticket_ptr =
                            Owning_ptr<an_invocation_ticket, Direct_allocator>;
                        /* The type used for an invocation ticket managed
                           by an owning pointer. */

using a_daemon_string = Allocated_string<Direct_allocator>;
                        /* The type used for a string in the daemon. */

template<typename a_Type>
using Daemon_dyn_array = Dyn_array<a_Type, Direct_allocator>;
                        /* The type used for a Dyn_array<T> in the daemon. */


static Opt<a_daemon_string> read_into_string(a_socket_reader *reader)
/*
Given a socket reader instance, read the next message from the socket.  If the
message was completely received, return the message; otherwise, return an empty
optional.
*/
{
  Opt<a_daemon_string> result;
  a_daemon_string      result_str;
  auto                 print_contents = [&result_str]
                                            (const char *msg, size_t msg_len) {
    result_str.append(a_string_view(msg, msg_len));
  };

  if (reader->read_message_parts(print_contents)) {
    result = move_from(&result_str);
  }  /* if */
  return result;
}  /* read_into_string */

#if DEBUG
/* GCOVR_EXCL_START */

static thread_local a_daemon_string
                trans_unit_tag;
                        /* An identifying tag passed from the client.  This is
                           used to allow the client to provide some useful
                           identifying tag to associate a specific client with
                           the server (daemon) thread. */

static thread_local uintptr_t
                trans_unit_tag_hash;
                        /* A hash of trans_unit_tag; this can be useful when
                           using debuggers that capture a replayable execution
                           snapshot to apply break points to a specific
                           translation unit (without the more significant
                           performance impact of string comparison). */


uintptr_t hash_trans_unit_tag(a_const_char *str)
/*
This function exists to allow computing the trans_unit_tag_hash in a debugger
with ease.  The function intentionally has external linkage to avoid complaints
about an unused function when CHECKING is FALSE.
*/
{
  return hash_ptr(a_string_view(str));
}  /* hash_trans_unit_tag */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

static thread_local int
                fd_thread_output;
                        /* The default output file descriptor for the current
                           thread. */


static FILE* get_new_output_file_for_thread()
/*
Return a new output FILE for the current thread.
*/
{
  FILE *result;
  int  fd = dup(fd_thread_output);

  if (fd == -1) {
    fputs("The daemon has run out of available file descriptors.\n", stderr);
    exit(RC_CATASTROPHE);
  }  /* if */
  result = fdopen(fd, "w");
  /* Do not buffer output; this ensures output is immediately forwarded without
     explicit fflush calls. */
  setbuf(result, NULL);
  return result;
}  /* get_new_output_file_for_thread */


FILE* default_error_output_file()
/*
Return the default error file for the current thread.
*/
{
  return get_new_output_file_for_thread();
}  /* default_error_output_file */


FILE* default_preproc_output_file()
/*
Return the preprocessor output file for the current thread.
*/
{
  return get_new_output_file_for_thread();
}  /* default_preproc_output_file */

#if NEED_IL_DISPLAY

FILE* default_il_display_output_file()
/*
Return the default IL display output file for the current thread.
*/
{
  return get_new_output_file_for_thread();
}  /* default_il_display_output_file */

#endif /* NEED_IL_DISPLAY */

static void process_ticket(const an_owned_invocation_ticket_ptr &ticket)
/*
Invoke the front end's main function with the information provided via the
invocation ticket.
*/
{
  Opt<a_daemon_string> opt_request_tag;
  {
    a_socket_reader reader(ticket->communication_fd);

    /* Figure out what kind of request is being processed. */
    opt_request_tag = read_into_string(&reader);
    if (!opt_request_tag.has_value()) {
      goto client_read_failure;
    }  /* if */
    if (*opt_request_tag == exec_request_bytes) {
      /* This is the usual case, executing the front end. */
    } else if (*opt_request_tag == ping_request_bytes) {
      /* The connected client is just checking to make sure the daemon is
         alive; discard the ticket. */
      goto done;
    } else {
      /* This is not a recognized daemon protocol request tag. */
      goto unknown_request_tag;
    }  /* if */

    /* Update the thread state. */
#if DEBUG
/* GCOVR_EXCL_START */
    Opt<a_daemon_string> opt_tu_tag = read_into_string(&reader);

    if (!opt_tu_tag.has_value()) {
      goto client_read_failure;
    }  /* if */
    trans_unit_tag = *opt_tu_tag;
    trans_unit_tag_hash = hash_ptr(trans_unit_tag);
    /* If this assertion fails, the hashing algorithm used for a_daemon_string
       has diverged; this will result in it being difficult to use
       trans_unit_tag_hash.  Presumably if this happens, the
       hash_trans_unit_tag hashing strategy should be updated to mirror the new
       hashing strategy for a_daemon_string. */
    check_assertion(trans_unit_tag_hash ==
                    hash_trans_unit_tag(trans_unit_tag.as_temp_characters()));
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    fd_thread_output = ticket->communication_fd;

    Opt<a_daemon_string> opt_cmd_args = read_into_string(&reader);
    if (!opt_cmd_args.has_value()) {
      goto client_read_failure;
    }  /* if */

    a_const_char            *cmd_args = opt_cmd_args->as_temp_characters();
    /* The first string in the message should always be a null-terminated
       argument count. */
    int                     argc = atoi(cmd_args);
    Daemon_dyn_array<char*> argv;
    /* This is then followed by the actual arguments. */
    for (int i = 0; i < argc; ++i) {
      char *last_arg;

      if (argv.length() == 0) {
        last_arg = (char*)cmd_args;
      } else {
        last_arg = argv.back_elem();
      }  /* if */
      argv.push_back(last_arg + strlen(last_arg) + 1);
    }  /* for */

    int exit_code = EDG_MAIN(argc, argv.begin());
    /* Signal the end of output. */
    write(ticket->communication_fd, end_of_message_bytes,
          strlen(end_of_message_bytes));

    /* Write the exit code. */
    a_daemon_string exit_code_str(exit_code, end_of_message_bytes);
    write(ticket->communication_fd, exit_code_str.as_temp_characters(),
          exit_code_str.length());
  }
  goto done;
unknown_request_tag:
  fprintf(stderr, "Received bad request tag \"%s\" from client.\n",
          opt_request_tag->as_temp_characters());
  goto done;
client_read_failure:
  fputs("Received incomplete message from client.\n", stderr);
done:;
}  /* process_ticket */


static void *run_front_end_thread(void*)
/*
Run a front end thread that waits for work from the any_work pthread condition.
*/
{
  while (TRUE) {
    /* Acquire the any work lock and wait to be signaled that there's new
       work. */
    int acquired_lock = pthread_mutex_lock(&next_ticket_lock);

    if (acquired_lock != 0) {
      goto terminate_thread;
    }  /* if */
    /* Wait on the ticket becoming available. */
    while (next_ticket == NULL) {
      int wait_error = pthread_cond_wait(&any_work, &next_ticket_lock);
      if (wait_error != 0) {
        goto terminate_thread;
      }  /* if */
    }  /* while */

    /* Take the ticket. */
    an_owned_invocation_ticket_ptr current_ticket = next_ticket;
    next_ticket = NULL;

    /* Release the lock so more work can be assigned to other threads. */
    int released_lock = pthread_mutex_unlock(&next_ticket_lock);
    if (released_lock != 0) {
      goto terminate_thread;
    }  /* if */
    process_ticket(current_ticket);
  }  /* while */
terminate_thread:
  fputs("Thread shutting down.\n", stderr);
  return NULL;
}  /* run_front_end_thread */


static int determine_parallelism()
/*
Return the maximum number of threads to run in the daemon.

When MULTIPLE_THREAD_COMPILATION is TRUE: (for ease of implementation) this is
always the number of processors (i.e., threads) the host system has.

When MULTIPLE_THREAD_COMPILATION is FALSE: this is always one (to ensure only
one front end invocation occurs at a time within this process).
*/
{
#if MULTIPLE_THREAD_COMPILATION
  long num_processors = sysconf(_SC_NPROCESSORS_ONLN);

  /* Include an extra thread so that when clients assign one front end
     execution per system thread there is a thread available to answer
     pings. */
  return ((int)num_processors) + 1;
#else /* !MULTIPLE_THREAD_COMPILATION */
  return 1;
#endif /* MULTIPLE_THREAD_COMPILATION */
}  /* determine_parallelism */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/*
Note that the EDG namespace is not opened here.  Instead we do a
using-directive if the EDG code is in the edg namespace.
*/
USING_NAMESPACE_EDG

int main(int argc, char *argv[])
/*
The main routine for the front end Unix socket daemon.
*/
{
  int          return_value = 0;
  int          max_threads = determine_parallelism();
  pthread_t    *threads = new pthread_t[max_threads];
  a_const_char *socket_address = allocating_socket_file_name();
  int          init_progress = 0;

  next_ticket = NULL;
  { /* Create the threads that wait for work on a shared condition (any_work);
       this is done ahead of time to allow for threads to be reused rather than
       creating a new thread for each invocation and/or "searching" for the
       next available thread. */
    int next_ticket_lock_set_up = pthread_mutex_init(&next_ticket_lock,
                                                     /*attr=*/NULL);

    if (next_ticket_lock_set_up != 0) {
      goto threading_set_up_error;
    }  /* if */
    ++init_progress;

    int any_work_set_up = pthread_cond_init(&any_work, /*attr=*/NULL);
    if (any_work_set_up != 0) {
      goto threading_set_up_error;
    }  /* if */
    ++init_progress;

    /* Set up the threading attributes. */
    pthread_attr_t thread_attr;
    if (pthread_attr_init(&thread_attr) != 0) {
      goto threading_set_up_error;
    }  /* if */

    /* Set the thread's stack to approximately 10MiB (the normal stack size on
       most Linux main threads).  Some operating systems (e.g., MacOS) mandate
       that the thread stack size is a multiple of the page size; thus, the
       10MiB starting point is adjusted so as to be a multiple of the page
       size. */
    size_t stack_size = 10 * 1024 * 1024;
    size_t page_size = (size_t)getpagesize();
    size_t page_remainder = stack_size % page_size;
    if (page_remainder != 0) {
      stack_size -= page_remainder;
      stack_size += page_size;
    }  /* if */
    if (pthread_attr_setstacksize(&thread_attr, stack_size) != 0) {
      goto threading_set_up_error;
    }  /* if */
    for (int i = 0; i < max_threads; i++) {
      int thread_launch_result = pthread_create(&threads[i], &thread_attr,
                                                run_front_end_thread,
                                                /*arg=*/NULL);
      if (thread_launch_result != 0) {
        goto threading_set_up_error;
      }  /* if */
    }  /* for */
    /* Destroy the threading attributes. */
    if (pthread_attr_destroy(&thread_attr) != 0) {
      goto threading_set_up_error;
    }  /* if */

    /* Create a new Unix streaming (bi-directional) socket. */
    int server_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_socket_fd == -1) {
      goto socket_set_up_error;
    }  /* if */

    /* Attempt to remove any remaining named socket file from a previous daemon
       invocation. */
    (void)unlink(socket_address);

    /* Bind the socket (so that the client can find it) and begin listening for
       connections. */
    size_t      socket_path_bytes = strlen(socket_address) + 1;
    sockaddr_un server_socket_addr = {};
    if (socket_path_bytes > sizeof(server_socket_addr.sun_path)) {
      goto socket_path_too_long_error;
    }  /* if */
    server_socket_addr.sun_family = AF_UNIX;
    memcpy(server_socket_addr.sun_path, socket_address, socket_path_bytes);

    int bind_result = bind(server_socket_fd,
                           (const sockaddr*)&server_socket_addr,
                           sizeof(server_socket_addr));
    if (bind_result == -1) {
      goto socket_set_up_error;
    }  /* if */

    /* Determine how large of a backlog to offer dynamically based on the
       number of processors (i.e., threads) the host system supports.  This
       allows a reasonably sane number of requests to quickly hit the daemon
       before it applies back-pressure to the client (via the kernel denying
       the connection requests). */
    int backlog_size = max_threads * 2;
    int listen_result = listen(server_socket_fd, backlog_size);
    if (listen_result == -1) {
      goto socket_set_up_error;
    }  /* if */
    /* Begin accepting connections. */
    while (TRUE) {
      /* Listen for new work. */
      int       accept_socket_fd = accept(server_socket_fd, /*address=*/NULL,
                                          /*address_len=*/NULL);
      a_boolean try_again = TRUE;

      /* Attempt to assign the work. */
      do {
        int acquired_lock = pthread_mutex_lock(&next_ticket_lock);

        if (acquired_lock != 0) {
          goto coordination_lock_error;
        }  /* if */
        if (next_ticket == NULL) {
          /* Allocate a new ticket; it will be freed by the thread that accepts
             the work. */
          next_ticket = new_direct<an_invocation_ticket>();
          next_ticket->communication_fd = accept_socket_fd;
          try_again = FALSE;
        }  /* if */
        /* If there's a sleeping worker thread available, let it know there's
           work to be done. */
        pthread_cond_signal(&any_work);

        int released_lock = pthread_mutex_unlock(&next_ticket_lock);
        if (released_lock != 0) {
          goto coordination_lock_error;
        }  /* if */
      } while (try_again);
    }  /* while */
  }
  goto done;
threading_set_up_error:
  fputs("Thread set up failed.\n", stderr);
  return_value = 1;
  goto done;
socket_set_up_error:
  fputs("Unix socket set up failed.\n", stderr);
  fprintf(stderr, "Attempting to use socket: %s\n", socket_address);
  return_value = 1;
  goto done;
socket_path_too_long_error:
  fprintf(stderr, "Unix socket path (\"%s\") is too long.\n", socket_address);
  return_value = 1;
  goto done;
coordination_lock_error:
  fputs("Coordination lock failure.\n", stderr);
  return_value = 1;
  goto done;
done:
  if (init_progress > 1) {
    pthread_cond_destroy(&any_work);
  }  /* if */
  if (init_progress > 0) {
    pthread_mutex_destroy(&next_ticket_lock);
  }  /* if */
  delete next_ticket;
  delete [] threads;
  delete [] socket_address;
  return return_value;
}  /* main */

#endif /* EDG_FRONT_END_DAEMON */

