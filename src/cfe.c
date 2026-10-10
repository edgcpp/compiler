/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

cfe.c -- Main program for C++/C front end.

C front end written by J. Stephen Adamczyk and Eric Schwarz, 1988-1989.
Changed to C++ front end and developed and enhanced at Edison Design Group by
  J. Stephen Adamczyk 1991-2018
  R. Michael Anderson 1991-1999
  John H. Spicer      1992-2026
  Daveed Vandevoorde  1999-2026
  William M. Miller   2004-2026
  Michael J. Herrick  2006-2026
  Ellen Herrick       2018-2026
  Nina Ranns          2018-2019
  Caleb Sunstrum      2019-2022
  Wyatt Childers      2021-2026
  Christof Meerwald   2022-2026
Open-sourced in September of 2026.
*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "fe_init.h"
#include "fe_wrapup.h"
#if MAKE_FRONT_END_CALLABLE
#include "mem_manage.h"
#endif /* MAKE_FRONT_END_CALLABLE */

#if BACK_END_IS_C_GEN_BE
#include "c_gen_be.h"
#endif /* BACK_END_IS_C_GEN_BE */
#if BACK_END_IS_CP_GEN_BE
#include "cp_gen_be.h"
#endif /* BACK_END_IS_CP_GEN_BE */
#if BACK_END_IS_LLVM_GEN_BE
#include "llvm_gen_be.h"
#endif /* BACK_END_IS_LLVM_GEN_BE */
#if BACK_END_SHOULD_BE_CALLED && \
    !BACK_END_IS_C_GEN_BE && !BACK_END_IS_CP_GEN_BE && !BACK_END_IS_LLVM_GEN_BE
/*
Provide a declaration for a non-EDG-supplied back end (not in the "edg"
namespace).
*/
extern void back_end(void);
#endif /* BACK_END_SHOULD_BE_CALLED && !BACK_END_IS_C_GEN_BE && !... */

/*
Note that the EDG namespace is not opened here.  Instead we do a
using-directive if the EDG code is in the edg namespace.
*/
USING_NAMESPACE_EDG

NORETURN static void cfe_main(int argc, char *argv[])
/*
This routine does the actual work to perform a compilation.  This is
called by the EDG_MAIN wrapper that performs error handling when
MAKE_FRONT_END_CALLABLE is TRUE.
*/
{
  an_error_severity most_severe_diagnostic, diagnostic_level;
  a_timer	    start_time;
  a_timer	    fe_start_time;
  a_timer	    fe_end_time;
#if USING_KAI_INLINER
  a_timer	    opt_start_time;
  a_timer	    opt_end_time;
#endif /* USING_KAI_INLINER */
  a_timer	    module_start_time;
  a_timer	    module_end_time;
#if BACK_END_SHOULD_BE_CALLED
  a_timer	    be_start_time;
  a_timer	    be_end_time;
#endif /* BACK_END_SHOULD_BE_CALLED */
  a_timer	    end_time;

  /* Initialize the file variable used for error output.  This should be
     done before anything else that could potentially produce error output
     (including an internal error). */
  f_error = default_error_output_file();
#if DEBUG
  /* Initialize the file variable used for debug output.  This should be
     done before anything else that could potentially produce debug output. */
  f_debug = f_error;
#endif /* DEBUG */
  /* Do early (before command-line processing) initialization. */
  fe_early_init();
#if BACK_END_SHOULD_BE_CALLED && BACK_END_IS_CP_GEN_BE && \
    MAKE_FRONT_END_CALLABLE
  /* When using the C++-generating back end with the front end as a library,
     ensure that early initialization of the back end occurs.  This in turn
     ensures that the back end can be cleaned up properly in fe_cleanup (even
     if it wasn't invoked during this compilation). */
  cp_gen_be_early_init();
#endif /* BACK_END_SHOULD_BE_CALLED && BACK_END_IS_CP_GEN_BE && ... */
  /* Get the execution starting time.  Do this unconditionally because the
     timing command line option will not have been processed yet.  This must
     be done after the early initialization done above. */
  get_timer(&start_time);
  /* Process the command line. */
  proc_command_line(argc, argv);
  /* Initialize values that apply to the entire compilation if multiple
     files are allowed. */
  fe_one_time_init();
#if COMPILE_MULTIPLE_SOURCE_FILES
  /* Loop if multiple source files are allowed. */
  most_severe_diagnostic = es_none;
  do {
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */
    /* Get the front end starting time. */
    if (display_compilation_time) get_timer(&fe_start_time);
    /* Initialize the per-compilation variables related to translation
       unit processing. */
    trans_unit_init();
    /* Process the source file. */
    process_translation_unit(primary_source_file_name, /*is_primary=*/TRUE,
                             (an_exported_template_file_ptr)NULL);
    /* Run module generation if required. */
    if (create_module_unit) {
      if (display_compilation_time) get_timer(&module_start_time);
      modules_write_out();
      if (display_compilation_time) {
        get_timer(&module_end_time);
        /* Display the amount of time used during generation of the module
           files for this translation unit. */
        display_time_used("Module file generation", &module_start_time,
                          &module_end_time);
      }  /* if */
    }  /* if */
    /* Do wrap-up processing for the front end (before the back end). */
    fe_wrapup();
    if (display_compilation_time) {
      /* Get the front end ending time. */
      get_timer(&fe_end_time);
      /* Display the amount of time used by the front end. */
      display_time_used("Front end time", &fe_start_time, &fe_end_time);
    }  /* if */
#if BACK_END_SHOULD_BE_CALLED
    /* Run the back end if required. */
#ifndef CALL_BACK_END_EVEN_WITH_ERRORS
    /* Do not run the back end if there were errors. */
    if (is_at_least_one_error()) suppress_back_end = TRUE;
#endif /* ifndef CALL_BACK_END_EVEN_WITH_ERRORS */
    if (!suppress_back_end) {
#if USING_KAI_INLINER
      /* Call the Kuck & Associates inliner (if being used).  It is
         not part of the source code provided by EDG. */
      extern void DoKfrontPasses();
      if (display_compilation_time) get_timer(&opt_start_time);
      DoKfrontPasses();
      if (display_compilation_time) {
        /* Get the back end start time. */
        get_timer(&opt_end_time);
        /* Display the amount of time used by the optimizer. */
        display_time_used("Optimizer time", &opt_start_time, &opt_end_time);
      }  /* if */
#endif /* USING_KAI_INLINER */
      if (display_compilation_time) get_timer(&be_start_time);
      back_end();
      if (display_compilation_time) {
        /* Get the back end ending time. */
        get_timer(&be_end_time);
        /* Display the amount of time used by the back end. */
        display_time_used("Back end time", &be_start_time, &be_end_time);
      }  /* if */
    }  /* if */
#endif /* BACK_END_SHOULD_BE_CALLED */

    /* Do wrap-up processing for the front end (after the back end). */
    fe_wrapup_part_2();

    /* Determine the most severe diagnostic encountered.  Catastrophic
       errors do not get here and therefore need not be checked for.
       Remarks don't count. */
    if (is_at_least_one_error()) {
      diagnostic_level = es_error;
    } else if (is_at_least_one_warning()) {
      diagnostic_level = es_warning;
    } else {
      diagnostic_level = es_none;
    }  /* if */
#if COMPILE_MULTIPLE_SOURCE_FILES
    if ((int)diagnostic_level > (int)most_severe_diagnostic) {
      most_severe_diagnostic = diagnostic_level;
    }  /* if */
    /* Keep compiling as long as there are more source files on the command
       line. */
  } while (get_next_source_file());
#else /* !COMPILE_MULTIPLE_SOURCE_FILES */
  most_severe_diagnostic = diagnostic_level;
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */

  if (display_compilation_time) {
    /* Get the ending time. */
    get_timer(&end_time);
    /* Display the amount of time used by the entire compilation. */
    display_time_used("Total compilation time", &start_time, &end_time);
  }  /* if */

  /* Exit with the return code appropriate to the highest severity error
     detected. */
  exit_compilation(most_severe_diagnostic);
}  /* cfe_main */

#if MAKE_FRONT_END_CALLABLE
BEGIN_EDG_NAMESPACE
#endif /* MAKE_FRONT_END_CALLABLE */

int EDG_MAIN(int argc, char *argv[]) /*lint !e759*/
/*
The main routine for the front end.

When MAKE_FRONT_END_CALLABLE is FALSE, the front end exits at the end
of the compilation with an exit status that indicates the status of
the compilation.  When MAKE_FRONT_END_CALLABLE is TRUE, the exit
status is returned to the caller.
*/
{
#if !MAKE_FRONT_END_CALLABLE
  cfe_main(argc, argv);
#else /* MAKE_FRONT_END_CALLABLE */
  if (setjmp(edg_main_setjmp_buffer) == 0) {
    cfe_main(argc, argv);
  }  /* if */
  /* Free all resources used by the compilation. */
  fe_cleanup();
  return exit_status;
#endif /* !MAKE_FRONT_END_CALLABLE */
}  /* EDG_MAIN */

#if MAKE_FRONT_END_CALLABLE
END_EDG_NAMESPACE
#endif /* MAKE_FRONT_END_CALLABLE */


