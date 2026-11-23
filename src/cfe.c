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
#if BACK_END_IS_GCC_GEN_BE
#include "gcc_gen_be_main.h"
#include "gcc_gen_be_context.h"
#endif /* BACK_END_IS_GCC_GEN_BE */
#if !BACK_END_IS_CP_GEN_BE
#include "edg_be_driver.h"
#include "edg_be_sdk.h"
#ifdef _WIN32
#include <process.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
#endif /* !BACK_END_IS_CP_GEN_BE */
#if BACK_END_SHOULD_BE_CALLED && \
    !BACK_END_IS_C_GEN_BE && !BACK_END_IS_CP_GEN_BE && \
    !BACK_END_IS_LLVM_GEN_BE && !BACK_END_IS_GCC_GEN_BE
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

#if !BACK_END_IS_CP_GEN_BE
/**
 * @brief Spawns a child `cpfe`/`edg++` driver process for isolated per-TU compilation or linking.
 * @details Uses `_spawnvp` on Windows and `fork`/`execvp`/`waitpid` on POSIX platforms.
 * @param[in] sub_argv Null-terminated argument vector starting with the driver executable path.
 * @return `edg_be_driver_error_t::ok` if the child exited with status 0, `invalid_argument` if
 *         `sub_argv` or `sub_argv[0]` is null, or `io_error` on spawn/child failure.
 */
EDG_BE_NODISCARD static edg_be_driver_error_t spawn_driver_subcommand(
    char *const *sub_argv) noexcept
{
  if (sub_argv == NULL || sub_argv[0] == NULL) {
    return edg_be_driver_error_t::invalid_argument;
  }
#ifdef _WIN32
  intptr_t rc = _spawnvp(_P_WAIT, sub_argv[0], (const char *const *)sub_argv);
  return (rc == 0) ? edg_be_driver_error_t::ok : edg_be_driver_error_t::io_error;
#else
  pid_t pid = fork();
  if (pid < 0) return edg_be_driver_error_t::io_error;
  if (pid == 0) {
    execvp(sub_argv[0], sub_argv);
    _exit(127);
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) return edg_be_driver_error_t::io_error;
  return (WIFEXITED(status) && WEXITSTATUS(status) == 0)
             ? edg_be_driver_error_t::ok
             : edg_be_driver_error_t::io_error;
#endif
}  /* spawn_driver_subcommand */
#endif /* !BACK_END_IS_CP_GEN_BE */

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
#if BACK_END_SHOULD_BE_CALLED && BACK_END_IS_GCC_GEN_BE && \
    MAKE_FRONT_END_CALLABLE
  gcc_gen_be_error_t err = gcc_gen_be_early_init();
  if (err != GCC_GEN_BE_SUCCESS) {
    if (gcc_gen_be_report_diagnostic(err, "gcc_gen_be_early_init failed") !=
        GCC_GEN_BE_SUCCESS) {
      fprintf(f_error, "gcc_gen_be_early_init failed\n");
    }
    internal_error("gcc_gen_be_early_init failed");
  }
#endif /* BACK_END_SHOULD_BE_CALLED && BACK_END_IS_GCC_GEN_BE && ... */
  /* Get the execution starting time.  Do this unconditionally because the
     timing command line option will not have been processed yet.  This must
     be done after the early initialization done above. */
  get_timer(&start_time);
#if !BACK_END_IS_CP_GEN_BE
  static edg_be_translated_command_t s_translated_cmd;
  a_boolean used_driver_translation = FALSE;
#if BACK_END_IS_LLVM_GEN_BE || BACK_END_IS_GCC_GEN_BE
  a_boolean bypass_driver_for_edg_base = FALSE;
#else
  a_boolean bypass_driver_for_edg_base = TRUE;
#endif
  {
    const char *edg_base_env = getenv("EDG_BASE");
    if (argc > 0 && argv != NULL && argv[0] != NULL) {
      const char *prog = argv[0];
      const char *slash = strrchr(prog, '/');
#ifdef _WIN32
      const char *bslash = strrchr(prog, '\\');
      if (bslash != NULL && (slash == NULL || bslash > slash)) slash = bslash;
#endif
      const char *base_prog = (slash != NULL) ? (slash + 1) : prog;
      if (strncmp(base_prog, "edg++", 5U) == 0 ||
          strncmp(base_prog, "cpfe++", 6U) == 0) {
        bypass_driver_for_edg_base = FALSE;
      } else {
        if (edg_base_env != NULL && edg_base_env[0] != '\0') {
          bypass_driver_for_edg_base = TRUE;
        }
        if (bypass_driver_for_edg_base) {
          for (int ai = 1; ai < argc; ++ai) {
            const char *arg = argv[ai];
            if (arg == NULL) continue;
            if (strncmp(arg, "--backend", 9U) == 0 ||
                strncmp(arg, "--llvm", 6U) == 0 ||
                strcmp(arg, "--gcc-be") == 0 ||
                strcmp(arg, "--gcc_gen_be") == 0 ||
                strcmp(arg, "--c-be") == 0 ||
                strcmp(arg, "--c_gen_be") == 0 ||
                strcmp(arg, "-print-search-dirs") == 0 ||
                strcmp(arg, "-print-resource-dir") == 0 ||
                strcmp(arg, "-print-target-triple") == 0 ||
                strcmp(arg, "-dumpmachine") == 0 ||
                strcmp(arg, "-dumpversion") == 0) {
              bypass_driver_for_edg_base = FALSE;
              break;
            }
          }
        }
      }
    }
  }
  if (!bypass_driver_for_edg_base) {
    edg_be_sdk_probe_env_t sdk_env;
    if (edg_be_sdk_probe_env_init(&sdk_env) == edg_be_driver_error_t::ok &&
        edg_be_translate_command_line(argc, argv, &sdk_env,
                                      &s_translated_cmd) ==
            edg_be_driver_error_t::ok) {
      if (s_translated_cmd.options.query_kind != edg_driver_query_kind_t::none) {
        const edg_be_sdk_result_t *sdk_ptr =
            s_translated_cmd.sdk_discovered ? &s_translated_cmd.sdk_info : NULL;
        if (edg_be_execute_driver_query(&s_translated_cmd.options, sdk_ptr,
                                        stdout) != edg_be_driver_error_t::ok) {
          exit_compilation(es_catastrophe);
        }
        exit_compilation(es_none);
      }
#if !BACK_END_IS_LLVM_GEN_BE
      if (s_translated_cmd.options.selected_backend ==
          edg_backend_kind_t::llvm_be) {
        fprintf(f_error,
                "Command-line error: LLVM backend is not enabled in this build\n");
        exit_compilation(es_command_line_error);
      }
#endif
#if !BACK_END_IS_GCC_GEN_BE
      if (s_translated_cmd.options.selected_backend ==
          edg_backend_kind_t::gcc_be) {
        fprintf(f_error,
                "Command-line error: GCC backend is not enabled in this build\n");
        exit_compilation(es_command_line_error);
      }
#endif
      if (s_translated_cmd.options.num_input_source_files == 0U &&
          s_translated_cmd.options.num_input_object_files > 0U) {
#if BACK_END_IS_LLVM_GEN_BE || BACK_END_IS_GCC_GEN_BE
        const edg_be_invocation_options_t *lopts = &s_translated_cmd.options;
        const char *link_out = (lopts->output_file_path != NULL)
                                   ? lopts->output_file_path
                                   : "a.out";
        C_dialect = lopts->language_is_cplusplus ? C_dialect_cplusplus
                                                 : C_dialect_ANSI;
        a_boolean link_gcc = FALSE;
#if BACK_END_IS_GCC_GEN_BE
        if (lopts->selected_backend == edg_backend_kind_t::gcc_be
#if !BACK_END_IS_LLVM_GEN_BE
            || lopts->selected_backend == edg_backend_kind_t::auto_select
#endif
            ) {
          link_gcc = TRUE;
        }
#endif
        if (link_gcc) {
#if BACK_END_IS_GCC_GEN_BE
          if (gcc_gen_be_apply_invocation_options(lopts) != GCC_GEN_BE_SUCCESS ||
              gcc_gen_be_link_multi_tu_artifacts(
                  (const char **)lopts->input_object_files,
                  (int)lopts->num_input_object_files,
                  link_out, 0, NULL) != GCC_GEN_BE_SUCCESS) {
            exit_compilation(es_error);
          }
          exit_compilation(es_none);
#endif
        } else {
#if BACK_END_IS_LLVM_GEN_BE
          ofile_name = link_out;
          if (llvm_apply_invocation_options(lopts) != llvm_gen_be_error_t::ok ||
              llvm_finalize_multi_tu_link() != llvm_gen_be_error_t::ok) {
            exit_compilation(es_error);
          }
          exit_compilation(es_none);
#endif
        }
#else
        exit_compilation(es_command_line_error);
#endif
      }
      if (!s_translated_cmd.native_edg_mode &&
          s_translated_cmd.options.num_input_source_files > 1U) {
        const edg_be_invocation_options_t *mopts = &s_translated_cmd.options;
        static char *s_base_flags[EDG_BE_DRIVER_MAX_ARGS];
        static char *s_sub_argv[EDG_BE_DRIVER_MAX_ARGS];
        static char s_mtu_objs[128][512];
        size_t nb = 0U;
        s_base_flags[nb++] = argv[0];
        for (size_t ti = 1U; ti < s_translated_cmd.num_expanded_tokens; ++ti) {
          const char *tok = s_translated_cmd.expanded_tokens[ti];
          a_boolean is_pos = FALSE;
          for (size_t si = 0U; si < mopts->num_input_source_files; ++si) {
            if (strcmp(tok, mopts->input_source_files[si]) == 0) {
              is_pos = TRUE;
              break;
            }
          }
          for (size_t oi = 0U; !is_pos && oi < mopts->num_input_object_files; ++oi) {
            if (strcmp(tok, mopts->input_object_files[oi]) == 0) {
              is_pos = TRUE;
              break;
            }
          }
          if (is_pos) continue;
          if (strcmp(tok, "-o") == 0) { ++ti; continue; }
          if (strncmp(tok, "-o", 2U) == 0 && tok[2] != '\0' && tok[2] != '-') {
            continue;
          }
          if (strcmp(tok, "-MF") == 0 || strcmp(tok, "-MT") == 0 ||
              strcmp(tok, "-MQ") == 0) {
            ++ti;
            continue;
          }
          if (strcmp(tok, "-MD") == 0 || strcmp(tok, "-MMD") == 0) continue;
          if (nb + 8U < EDG_BE_DRIVER_MAX_ARGS) {
            s_base_flags[nb++] = (char *)tok;
          }
        }
        a_boolean do_link =
            (mopts->artifact_kind == edg_output_artifact_kind_t::executable ||
             mopts->artifact_kind == edg_output_artifact_kind_t::shared_library)
                ? TRUE
                : FALSE;
        const char *tdir = (mopts->temp_work_dir != NULL && mopts->temp_work_dir[0] != '\0')
                               ? mopts->temp_work_dir
                               : ".";
        int any_fail = 0;
        size_t n_emitted = 0U;
        for (size_t si = 0U; si < mopts->num_input_source_files && si < 128U; ++si) {
          size_t k = 0U;
          for (size_t bi = 0U; bi < nb; ++bi) s_sub_argv[k++] = s_base_flags[bi];
          if (do_link) {
            snprintf(s_mtu_objs[si], sizeof(s_mtu_objs[si]),
                     "%s/.edg_mtu_%ld_%u.o", tdir,
#ifdef _WIN32
                     0L,
#else
                     (long)getpid(),
#endif
                     (unsigned)si);
            s_sub_argv[k++] = (char *)"-c";
            s_sub_argv[k++] = (char *)mopts->input_source_files[si];
            s_sub_argv[k++] = (char *)"-o";
            s_sub_argv[k++] = s_mtu_objs[si];
            ++n_emitted;
          } else {
            s_sub_argv[k++] = (char *)mopts->input_source_files[si];
          }
          s_sub_argv[k] = NULL;
          if (spawn_driver_subcommand(s_sub_argv) !=
              edg_be_driver_error_t::ok) {
            any_fail = 1;
            break;
          }
        }
        if (!any_fail && do_link) {
          size_t k = 0U;
          for (size_t bi = 0U; bi < nb; ++bi) s_sub_argv[k++] = s_base_flags[bi];
          for (size_t si = 0U; si < n_emitted; ++si) {
            s_sub_argv[k++] = s_mtu_objs[si];
          }
          for (size_t oi = 0U; oi < mopts->num_input_object_files; ++oi) {
            s_sub_argv[k++] = (char *)mopts->input_object_files[oi];
          }
          s_sub_argv[k++] = (char *)"-o";
          s_sub_argv[k++] = (char *)(mopts->output_file_path != NULL
                                         ? mopts->output_file_path
                                         : "a.out");
          s_sub_argv[k] = NULL;
          if (spawn_driver_subcommand(s_sub_argv) !=
              edg_be_driver_error_t::ok) {
            any_fail = 1;
          }
        }
        if (do_link) {
          for (size_t si = 0U; si < n_emitted; ++si) {
            if (remove(s_mtu_objs[si]) != 0) {
              /* Best-effort cleanup. */
            }
          }
        }
        exit_compilation(any_fail ? es_error : es_none);
      }
      used_driver_translation = TRUE;
    }
  }
  /* Process the command line. */
  if (used_driver_translation) {
    proc_command_line(s_translated_cmd.edg_argc, s_translated_cmd.edg_argv);
#if BACK_END_IS_LLVM_GEN_BE
    if (llvm_apply_invocation_options(&s_translated_cmd.options) !=
        llvm_gen_be_error_t::ok) {
      diagnostic_counters.total.errors++;
    }
#endif /* BACK_END_IS_LLVM_GEN_BE */
#if BACK_END_IS_GCC_GEN_BE
    if (gcc_gen_be_apply_invocation_options(&s_translated_cmd.options) !=
        GCC_GEN_BE_SUCCESS) {
      diagnostic_counters.total.errors++;
    }
#endif /* BACK_END_IS_GCC_GEN_BE */
  } else {
    proc_command_line(argc, argv);
  }
#else /* BACK_END_IS_CP_GEN_BE */
  /* Process the command line. */
  proc_command_line(argc, argv);
#endif /* !BACK_END_IS_CP_GEN_BE */
  /* Initialize values that apply to the entire compilation if multiple
     files are allowed. */
#if !BACK_END_IS_CP_GEN_BE && DO_IL_LOWERING && IA64_ABI && \
    (BACK_END_IS_LLVM_GEN_BE || BACK_END_IS_GCC_GEN_BE)
  {
    a_boolean native_be_selected = FALSE;
#if BACK_END_IS_LLVM_GEN_BE
    if (llvm_be_enabled) native_be_selected = TRUE;
#endif
#if BACK_END_IS_GCC_GEN_BE
    if (gcc_be_enabled) native_be_selected = TRUE;
#endif
    if (used_driver_translation &&
        s_translated_cmd.options.selected_backend !=
            edg_backend_kind_t::c_gen_be) {
      native_be_selected = TRUE;
    }
    if (native_be_selected && targ_sizeof_pointer == 8) {
      targ_ia64_abi_use_int_static_init_guard = FALSE;
    }
  }
#endif
  fe_one_time_init();
#if !BACK_END_IS_CP_GEN_BE && (BACK_END_IS_LLVM_GEN_BE || BACK_END_IS_GCC_GEN_BE)
  unsigned tu_ordinal = 0U;
#endif
#if !BACK_END_IS_CP_GEN_BE && BACK_END_IS_GCC_GEN_BE
  static char s_gcc_tu_objs_storage[128][512];
  static const char *s_gcc_link_objs[640];
  size_t num_gcc_tu_objs = 0U;
#endif
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
#if !BACK_END_IS_CP_GEN_BE && (BACK_END_IS_LLVM_GEN_BE || BACK_END_IS_GCC_GEN_BE)
    static char s_cwd_obj_buf[512];
    if (used_driver_translation && primary_source_file_name != NULL) {
      const edg_be_invocation_options_t *dopts = &s_translated_cmd.options;
      if (dopts->artifact_kind == edg_output_artifact_kind_t::object_file) {
        const char *chosen_obj = NULL;
        if (dopts->output_file_path != NULL &&
            dopts->num_input_source_files <= 1U) {
          chosen_obj = dopts->output_file_path;
        } else if (edg_be_derive_cwd_object_name(
                       primary_source_file_name, false, s_cwd_obj_buf,
                       sizeof(s_cwd_obj_buf)) == edg_be_driver_error_t::ok) {
          chosen_obj = s_cwd_obj_buf;
        }
#if BACK_END_IS_LLVM_GEN_BE
        if (chosen_obj != NULL) gen_obj_file_name = chosen_obj;
#endif /* BACK_END_IS_LLVM_GEN_BE */
#if BACK_END_IS_GCC_GEN_BE
        if (chosen_obj != NULL) gcc_be_output_file_name = chosen_obj;
#endif /* BACK_END_IS_GCC_GEN_BE */
      } else if ((dopts->num_input_source_files > 1U ||
                  dopts->num_input_object_files > 0U) &&
                 (dopts->artifact_kind == edg_output_artifact_kind_t::executable ||
                  dopts->artifact_kind == edg_output_artifact_kind_t::shared_library)) {
        char base_obj[256];
        if (edg_be_derive_cwd_object_name(primary_source_file_name, false,
                                          base_obj, sizeof(base_obj)) ==
            edg_be_driver_error_t::ok) {
          snprintf(s_cwd_obj_buf, sizeof(s_cwd_obj_buf), ".edg_tu_%u_%s",
                   tu_ordinal, base_obj);
#if BACK_END_IS_LLVM_GEN_BE
          ofile_name = (dopts->output_file_path != NULL)
                           ? dopts->output_file_path
                           : "a.out";
#endif /* BACK_END_IS_LLVM_GEN_BE */
#if BACK_END_IS_GCC_GEN_BE
          if (gcc_be_enabled && num_gcc_tu_objs < 128U) {
            snprintf(s_gcc_tu_objs_storage[num_gcc_tu_objs],
                     sizeof(s_gcc_tu_objs_storage[num_gcc_tu_objs]), "%s",
                     s_cwd_obj_buf);
            gcc_be_output_file_name = s_gcc_tu_objs_storage[num_gcc_tu_objs];
            gcc_be_compile_only = TRUE;
            ++num_gcc_tu_objs;
          }
#endif /* BACK_END_IS_GCC_GEN_BE */
        }
      }
    }
    ++tu_ordinal;
#endif /* !BACK_END_IS_CP_GEN_BE && (LLVM || GCC) */
    /* Process the source file. */
    process_translation_unit(primary_source_file_name, /*is_primary=*/TRUE,
                             (an_exported_template_file_ptr)NULL);
#if !BACK_END_IS_CP_GEN_BE
    if (used_driver_translation && s_translated_cmd.options.generate_depfile &&
        primary_source_file_name != NULL) {
      static const char *s_inc_deps[4096];
      static a_source_file_ptr s_sf_stack[1024];
      size_t inc_count = 0U;
      size_t sp = 0U;
      if (il_header.primary_source_file != NULL &&
          il_header.primary_source_file->first_child_file != NULL) {
        s_sf_stack[sp++] = il_header.primary_source_file->first_child_file;
      }
      while (sp > 0U) {
        a_source_file_ptr cur_sf = s_sf_stack[--sp];
        if (cur_sf->next != NULL && sp < 1024U) {
          s_sf_stack[sp++] = cur_sf->next;
        }
        if (cur_sf->is_include_file && cur_sf->full_name != NULL &&
            (s_translated_cmd.options.dep_system_headers ||
             !cur_sf->from_system_include_dir)) {
          a_boolean dup = FALSE;
          for (size_t di = 0U; di < inc_count; ++di) {
            if (strcmp(s_inc_deps[di], cur_sf->full_name) == 0) {
              dup = TRUE;
              break;
            }
          }
          if (!dup && inc_count < 4096U) {
            s_inc_deps[inc_count++] = cur_sf->full_name;
          }
        }
        if (cur_sf->first_child_file != NULL && sp < 1024U) {
          s_sf_stack[sp++] = cur_sf->first_child_file;
        }
      }
      if (edg_be_write_depfile(&s_translated_cmd, primary_source_file_name,
                               s_inc_deps, inc_count) != edg_be_driver_error_t::ok) {
        diagnostic_counters.total.errors++;
      }
    }
#endif /* !BACK_END_IS_CP_GEN_BE */
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
#if BACK_END_IS_CP_GEN_BE
      back_end();
#else /* !BACK_END_IS_CP_GEN_BE */
      {
        a_boolean run_c_be = FALSE;
        a_boolean run_gcc_be = FALSE;
        a_boolean run_llvm_be = FALSE;
#if BACK_END_IS_C_GEN_BE
        if (c_be_enabled) {
          run_c_be = TRUE;
        }
#endif /* BACK_END_IS_C_GEN_BE */
#if BACK_END_IS_GCC_GEN_BE
        if (!run_c_be && gcc_be_enabled) {
          run_gcc_be = TRUE;
        }
#endif /* BACK_END_IS_GCC_GEN_BE */
#if BACK_END_IS_LLVM_GEN_BE
        if (!run_c_be && !run_gcc_be && llvm_be_enabled) {
          run_llvm_be = TRUE;
        }
#endif /* BACK_END_IS_LLVM_GEN_BE */
        if (!run_c_be && !run_gcc_be && !run_llvm_be) {
#if BACK_END_IS_LLVM_GEN_BE
          run_llvm_be = TRUE;
#elif BACK_END_IS_GCC_GEN_BE
          run_gcc_be = TRUE;
#elif BACK_END_IS_C_GEN_BE
          run_c_be = TRUE;
#endif
        }
#if BACK_END_IS_C_GEN_BE
        if (run_c_be) {
          c_gen_be_entry();
        }
#endif /* BACK_END_IS_C_GEN_BE */
#if BACK_END_IS_GCC_GEN_BE
        if (run_gcc_be) {
          a_boolean saved_ife = in_front_end;
          a_translation_unit_ptr saved_tu = curr_translation_unit;
          if (translation_units != NULL) {
            in_front_end = TRUE;
            curr_translation_unit = translation_units;
          }
          gcc_gen_be_error_t be_err = gcc_gen_be();
          in_front_end = saved_ife;
          curr_translation_unit = saved_tu;
          if (be_err != GCC_GEN_BE_SUCCESS) {
            if (gcc_gen_be_report_diagnostic(be_err, "back_end failed") !=
                GCC_GEN_BE_SUCCESS) {
              /* Diagnostic status already matches be_err. */
            }
            internal_error("back_end failed");
          }
        }
#endif /* BACK_END_IS_GCC_GEN_BE */
#if BACK_END_IS_LLVM_GEN_BE
        if (run_llvm_be) {
          a_boolean saved_ife = in_front_end;
          a_translation_unit_ptr saved_tu = curr_translation_unit;
          if (translation_units != NULL) {
            in_front_end = TRUE;
            curr_translation_unit = translation_units;
          }
          llvm_gen_be_error_t be_err = llvm_gen_be();
          in_front_end = saved_ife;
          curr_translation_unit = saved_tu;
          if (be_err != llvm_gen_be_error_t::ok) {
            const char *err_str = NULL;
            const char *detail_str = NULL;
            if (llvm_gen_be_error_to_string(be_err, &err_str) !=
                llvm_gen_be_error_t::ok) {
              err_str = NULL;
            }
            if (llvm_gen_be_get_last_error_detail(&detail_str) !=
                llvm_gen_be_error_t::ok) {
              detail_str = NULL;
            }
            if (detail_str != NULL && detail_str[0] != '\0') {
              fprintf(f_error, "error: LLVM backend failure (%s): %s\n",
                      err_str != NULL ? err_str : "unknown", detail_str);
            } else {
              fprintf(f_error, "error: LLVM backend failure (%s)\n",
                      err_str != NULL ? err_str : "unknown");
            }
            diagnostic_counters.total.errors++;
          }
        }
#endif /* BACK_END_IS_LLVM_GEN_BE */
#if !BACK_END_IS_C_GEN_BE && !BACK_END_IS_LLVM_GEN_BE && !BACK_END_IS_GCC_GEN_BE
        back_end();
#endif
      }
#endif /* BACK_END_IS_CP_GEN_BE */
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

#if !BACK_END_IS_CP_GEN_BE
#if BACK_END_IS_LLVM_GEN_BE
  if (most_severe_diagnostic < es_error
#if BACK_END_IS_GCC_GEN_BE
      && !gcc_be_enabled
#endif /* BACK_END_IS_GCC_GEN_BE */
#if BACK_END_IS_C_GEN_BE
      && !c_be_enabled
#endif /* BACK_END_IS_C_GEN_BE */
      ) {
    llvm_gen_be_error_t link_err = llvm_finalize_multi_tu_link();
    if (link_err != llvm_gen_be_error_t::ok) {
      const char *err_str = NULL;
      if (llvm_gen_be_error_to_string(link_err, &err_str) !=
          llvm_gen_be_error_t::ok) {
        err_str = NULL;
      }
      fprintf(f_error, "error: LLVM multi-TU link failure (%s)\n",
              err_str != NULL ? err_str : "unknown");
      diagnostic_counters.total.errors++;
      most_severe_diagnostic = es_error;
    }  /* if */
  }  /* if */
#endif /* BACK_END_IS_LLVM_GEN_BE */
#if BACK_END_IS_GCC_GEN_BE
  if (most_severe_diagnostic < es_error && gcc_be_enabled &&
      num_gcc_tu_objs > 0U) {
    size_t total_objs = 0U;
    for (size_t gi = 0U; gi < num_gcc_tu_objs && total_objs < 640U; ++gi) {
      s_gcc_link_objs[total_objs++] = s_gcc_tu_objs_storage[gi];
    }
    for (size_t oi = 0U;
         oi < s_translated_cmd.options.num_input_object_files &&
         total_objs < 640U;
         ++oi) {
      s_gcc_link_objs[total_objs++] =
          s_translated_cmd.options.input_object_files[oi];
    }
    const char *gcc_final_out =
        (s_translated_cmd.options.output_file_path != NULL)
            ? s_translated_cmd.options.output_file_path
            : "a.out";
    gcc_gen_be_error_t gl_err = gcc_gen_be_link_multi_tu_artifacts(
        s_gcc_link_objs, (int)total_objs, gcc_final_out, 0, NULL);
    for (size_t gi = 0U; gi < num_gcc_tu_objs; ++gi) {
      if (remove(s_gcc_tu_objs_storage[gi]) != 0) {
        /* Best-effort cleanup of temporary object file. */
      }
    }
    if (gl_err != GCC_GEN_BE_SUCCESS) {
      diagnostic_counters.total.errors++;
      most_severe_diagnostic = es_error;
    }
  }
#endif /* BACK_END_IS_GCC_GEN_BE */
#endif /* !BACK_END_IS_CP_GEN_BE */

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

#if BACK_END_IS_GCC_GEN_BE && !MAKE_FRONT_END_CALLABLE
extern "C" {
/**
 * @brief Supplies LeakSanitizer suppression rules for external `libgccjit.so` singletons.
 * @details GCC's `libgccjit.so` retains one-time internal GGC/option tables across
 *          `gcc_jit_context_release`; suppressing `libgccjit.so` preserves full LSan
 *          checking on all EDG front-end and backend allocations.
 * @return Null-terminated newline-delimited LSan suppression specification.
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((visibility("default"), used))
#endif /* defined(__GNUC__) || defined(__clang__) */
const char *__lsan_default_suppressions(void) noexcept
{
  return "leak:libgccjit.so\n";
}  /* __lsan_default_suppressions */
}
#endif /* BACK_END_IS_GCC_GEN_BE && !MAKE_FRONT_END_CALLABLE */


