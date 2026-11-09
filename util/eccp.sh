#!/bin/sh

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Run the EDG C++ front end into the system cc to compile C++.
# Interface and command-line options are similar to CC.

# -o isn't supported on all platforms - but we don't need to adjust paths there
platform=`uname -o 2> /dev/null`
native_path()
#
# Function that takes a single argument (a path) and returns the native path
# for that argument.  On *nix systems, this is a nop, however on Windows
# systems this attempts to convert the path to a Windows style path.
# Currently only Cygwin path conversions are supported.
#
{
  path="$1"
  if [ "$platform" = "Cygwin" ] ; then
    path=`cygpath -m "$path"`
  fi
  echo "$path"
}

#
# These variables are used to check front end exit status.
#
any_errors=0
max_status=0

#
# Initialize EDG_BASE.  This needs to be done before looking for the
# config file below.
#
export EDG_BASE
EDG_BASE=${EDG_BASE-/edg/cpfe}
#
# Look for a file in EDG_BASE called edg_eccp_config.  If such a file
# exists, process it to initialize environment variables.
config_file=$EDG_BASE/edg_eccp_config
if [ -f $config_file ] ; then
	. $config_file
fi
#
# Predefined preprocessing variables.
#
defines=${EDG_DEFAULT_DEFINES-""}
c_defines=${EDG_DEFAULT_C_DEFINES-""}
cpp_defines=${EDG_DEFAULT_CPP_DEFINES-""}
EDG_CBASE=${EDG_CBASE-/edg/cpfe}
#
# The driver name to be used in diagnostics
#
driver_name=eccp
#
# EDG_DRIVER_VERSION is used to disable driver features that are incompatible
# with earlier versions of the front end.
#
driver_version=${EDG_DRIVER_VERSION-999}
#
# Default include directories.  The default directories are specified by
# EDG_DEFAULT_INCLUDE_DIRS.  If this variable is not set, then we
# select either INCLDIR or CINCLDIR depending on the language being
# compiled.  This is done after command line processing when we know
# the language being compiled.
#
# Directory where the C++ include files are to be found.
#
INCLDIR=${EDG_INCLDIR-$EDG_BASE/include}
#
# Directory where the C include files are to be found.
#
CINCLDIR=${EDG_CINCLDIR-$EDG_CBASE/include}
#
# Directory where libC.a is to be found.
#
LIBDIR=${ECCP_LIBDIR-$EDG_BASE/lib}
#
# Compiler executable
#
CPFE=${CPFE-$EDG_BASE/bin/cfe}
#
# "patch" executable
#
PATCH=${EDG_PATCH_PATH-$EDG_BASE/lib/patch}
#
# "munch" executable
#
MUNCH=${EDG_MUNCH_PATH-$EDG_BASE/lib/edg_munch}
#
# options to be passed to munch
#
EDG_MUNCH_OPTIONS=${EDG_MUNCH_OPTIONS-""}
#
# Flag indicating whether to use "patch" or "munch" for static initialization.
#
patch_mode=${EDG_PATCH_MODE-1}
#
# Options to be passed to the nm command when using munch
#
EDG_MUNCH_NM_OPTIONS=${EDG_MUNCH_NM_OPTIONS-""}
#
# "edg_prelink" executable
#
EDG_PRELINK=${EDG_PRELINK_PATH-$EDG_BASE/lib/edg_prelink}
#
# The option to be used to specify a library name
#
library_option=${EDG_LIBRARY_OPTION-"-L"}
#
# The option to be used to specify a library file
#
lib_file_opt=${EDG_LIBFILE_OPTION-"-l"}
#
# Default library paths of C to object compiler.  Used by the prelinker
# to find libraries specified with the -l option.
#
EDG_LINKER_LIB_PATHS=${EDG_LINKER_LIB_PATHS-"${library_option}/lib ${library_option}/usr/lib"}
#
# Library paths to be implicitly included in the prelinker and link commands
#
EDG_DEFAULT_LIB_PATHS=${EDG_DEFAULT_LIB_PATHS-""}
#
# Default options to the prelink command (no default value - use environment
# variable if set)
#
# EDG_PRELINK_DEFAULT_OPTIONS=$EDG_PRELINK_DEFAULT_OPTIONS
#
# edg_decode (demangler) executable.  If no edg_decode is available,
# use /bin/cat (or --no_demangle).  The script will work properly, you just
# won't get demangled names in linker output messages.
#
EDG_DECODE=${EDG_DECODE_PATH-$EDG_BASE/lib/edg_decode}
#
# A filter can be invoked to operate on the messages produced by
# the front end.  If EDG_CPFE_OUTPUT_FILTER is non-null, it is a command
# that is executed on the error output of the front end.
# EDG_CPFE_OUTPUT_FILTER_OPTIONS are the command line options passed
# to the filter.
#
EDG_CPFE_OUTPUT_FILTER=${EDG_CPFE_OUTPUT_FILTER-""}
EDG_CPFE_OUTPUT_FILTER_OPTIONS=${EDG_CPFE_OUTPUT_FILTER_OPTIONS-""}
#
# Flags that suppresses the implicit use of -tused under certain
# circumstances
#
EDG_NO_IMPLICIT_INSTANTIATE_USED=${EDG_NO_IMPLICIT_INSTANTIATE_USED-0}
#
# Flag indicating whether to do automatic instantiation by default
#
automatic_instantiation=1
#
# Flag indicating whether each instantiation should go into its own object
# file.
#
one_instantiation_per_object=0
#
# Flag indicating that the list of object files should be displayed.
#
list_object_files=0
#
# Default directory into which instantiations are placed when using
# one_instantiation_per_object.
#
instantiation_dir=Template.dir
use_default_instantiation_dir=1
#
# Directory to be used for temporary files.
#
export TMPDIR
TMPDIR=${TMPDIR-/tmp}
eccp_tmpdir=$(mktemp -d "${TMPDIR}/eccp.XXXXXXXX")
if [ $? -ne 0 ] ; then
  echo eccp: could not create temporary directory $eccp_tmpdir.
fi
eccp_tmpdir=`native_path "$eccp_tmpdir"`
#
# Extracted pattern portion of the tmpdir string.
#
eccp_tmp_pattern=${eccp_tmpdir##*.}
#
# Suffix to be applied to the standard C++ library (libC.a) to select a
# special version.
#
EDG_LIB_SUFFIX=${EDG_LIB_SUFFIX-""}
#
# Library names to be used on the link command.  This should be a colon
# separated list of library names of a form suitable for use with the
# -l options (e.g., std:xyz for -lstd and -lxyz).
#
EDG_STD_LIBS=${EDG_STD_LIBS-"std"}
#
# The name of the EDG runtime library to be passed to the linker.
#
EDG_RUNTIME_LIB=${EDG_RUNTIME_LIB-"C"}
#
# The name of the fixed-point library to be used when compiling
# embedded C.
#
EDG_FIXED_POINT_LIB=${EDG_FIXED_POINT_LIB-""}
#
# C compiler to use to compile the output and any options to be used with
# this compiler by default.
#
EDG_C_TO_OBJ_COMPILER=${EDG_C_TO_OBJ_COMPILER-cc}
EDG_C_TO_OBJ_DEFAULT_OPTIONS=${EDG_C_TO_OBJ_DEFAULT_OPTIONS--temp=$TMPDIR}
#
# Debug switch used by the compiler that's compiling the output
#
EDG_C_TO_OBJ_COMPILER_DEBUG_FLAG=${EDG_C_TO_OBJ_COMPILER_DEBUG_FLAG-"-g"}
#
# Flag that indicates that the generated C file should always be created
# in the current directory.  This provides compatibility with earlier
# versions of the front end that don't support the --gen_c_file_name
# option.
#
gen_c_in_curr_dir=${EDG_GEN_C_IN_CURR_DIR-0}
#
#   Get the name of the current directory
#
curr_dir=`pwd`
#
# Flag that indicates whether the path names used in -I options should be
# converted to absolute paths.  This should be set in environments where
# the --prelink_copy_if_nonlocal option is being used, and where the
# command line in the .ii file should reflect the directory to which the
# file was copied and not the original directory.
#
EDG_USE_ABSOLUTE_INCL_DIR_PATHS=${EDG_USE_ABSOLUTE_INCL_DIR_PATHS-0}
#
# Other variables used in automatic instantiation mode
#
if [ $automatic_instantiation -eq 1 ] ; then
  compile_command="$0"
fi
#
# Flag that indicates that the old instantiation information file
# format (without the current directory) should be used.
#
old_ii_format=${EDG_OLD_II_FORMAT-0}
#
# The suffix to be used on the generated C file and generated .o files.
#
gen_c_suffix=${EDG_GEN_C_SUFFIX-".int.c"}
EDG_GEN_O_SUFFIX=${EDG_GEN_O_SUFFIX-"o"}
gen_o_suffix=`expr "$gen_c_suffix" : '\(.*\)\.'`.$EDG_GEN_O_SUFFIX
#
# The flag for specifying an output file.  If no trailing space is provided,
# the output flag and output file will be adjacent.
#
output_flag=${EDG_OUTPUT_FLAG-"-o "}
#
# Default options to be passed to edgcpfe
#
EDG_CPFE_DEFAULT_OPTIONS=${EDG_CPFE_DEFAULT_OPTIONS-""}
#
# Set to TRUE if an eccp error (e.g., command line error) occurs.
#
error=0
#
# When set to 1, run front end and cc producing a .o file.
#
cc_only=0
#
# When set to 1, run front end producing a .int.c file.
#
fe_only=0
#
# When set to 1, use the native GCC (libgccjit) backend directly.
#
gcc_backend=${EDG_GCC_BACKEND-0}
#
# When set to 1, the front end is run in preprocessing mode only.
# Among other things, the front end will not create or remove the .ii
# file when only doing preprocessing
#
preprocessor_only=0
#
# When set to 1, preprocessor_only should be reset.  This is used to
# force compilation when using an option that usually suppresses
# compilation.  This is helpful when using the implicit inclusion option
# as it allows the compiler to produce a preprocessed output file that
# includes any implicitly included files.
#
suppress_preproc_only=0
#
# Name of the executable after linking.
#
executable=a.out
#
# Was a name for the output file explicitly specified.
#
output_file_specified=0
#
# A list of .c files to compile, separated by blanks.
#
cfiles=
more_than_one_c_file=0
#
# The file to compile as a module unit of the kind specified by
# module_unit_kind.
#
module_unit_src_file=
#
# When set to 1, the name of the module unit source file implies header unit
# creation.
#
module_unit_src_file_is_header=0
#
# The kind of module unit to create:
#
# 0 - no module unit
# 1 - header unit
# 2 - module interface unit
# 3 - module partition unit
#
module_unit_kind=0
#
# A list of the .o files, library files (e.g., .a files) and library 
# options (e.g., -la) to be passed to the linker.  The list is maintained
# in the sequence in which the source files, object files, and libraries
# are found on the command line
#
object_files=
#
# A list of .o files to be removed after linking.  These files are the ones
# that were added to the object_files list by compiling them.
#
rofiles=
#
# A list of -L options and archive files to be passed to the link step.
#
Loptions=
#
# Other options to be passed to the linker
#
ldoptions=
#
# Should the patch/munch phase be suppressed
#
suppress_patch_munch=${EDG_SUPPRESS_PATCH_MUNCH-0}
#
# Should the executable be stripped
#
strip_executable=0
#
# Path name of the strip command used to strip executables
#
STRIP=${EDG_STRIP_PATH-strip}
#
# Options to be passed to the underlying C compiler
#
c_to_obj_options=
#
# Were any library or object files specified on the command line?
#
any_l_or_o_files=0
#
# Were any source files specified on the command line?
#
any_c_files=0
#
# Were any module unit files specified on the command line?
#
any_module_unit_src_files=0
#
# If --multi_trans_unit mode is used, the secondary files specified on
# the command line.
#
secondary_files=
#
# A list of options to pass to front end.
#
feoptions=$defines
#
# If the generated C file suffix does not end with .c, then tell the front end
# to generate old style line commands.
#
if [ `basename $gen_c_suffix .c` = $gen_c_suffix ] ; then
        # This is true when the suffix does not end in .c
	feoptions=$feoptions" --old_line_commands"
fi
#
# Keep the generated C file
#
keep_int_file=0
#
# Should we link using the purify command?
#
link_using_purify=0
#
# Should we link using the quantify command?
#
link_using_quantify=0
#
# Flag indicating that C is being compiled instead of C++
#
c_mode=0
#
# Flag indicating that the standard include directory should be added.
#
std_incl=1
#
# Flag indicated that a instantiation mode was specified
#
instantiation_mode_specified=0
#
# Flag indicating that the --building_runtime option was specified (i.e., the
# runtime library is being built).
#
building_runtime=0
#
# Remove #line directives from the generated C
#
strip_line_dirs=0
#
# Suppress diagnostics from the underlying C compiler
#
suppress_c_to_object_diagnostics=0
#
# Options to be passed to the prelinker
#
prelink_options=
#
# Prelinker instantiation options.  prelink_local_only indicates
# that only local files (i.e., those in the current directory) are
# candidates for assignment of instantiations.  prelink_copy_if_nonlocal
# is TRUE if the assignment of an instantiation to a nonlocal object file
# should result in the object file being recompiled in the current
# directory.  use_definition_list_file specifies whether the prelinker
# should use a definition list file when invoking the front end.
#
prelink_local_only=0
prelink_copy_if_nonlocal=0
use_definition_list_file=1
#
# Run the prelinker (but not the linker) on the object files.
#
suppress_link=0
#
# Run the prelinker to cause instantiation flags to be removed
#
remove_instantiation_flags=0
#
# Show commands as they are executed
#
driver_debug=0
#
# Label driver debug commands as they are executed
#
verbose_driver_debug=0
#
# Special option for testing precompiled headers
#
pch_test_mode=0
#
# Special option for testing multiple translation unit processing
#
trans_unit_test_mode=0
#
# Another special mode for translation unit testing.  This one compiles
# the file as a secondary file then renames the output file to the
# expected name.
compile_as_secondary=0
dummy_primary_file_name=
remove_dummy_primary=0
#
# Indicates that multiple files should be compiled as translation units
# of a single compilation
#
multi_trans_unit=0
#
# Debug option that causes nm to be run on object files
#
nm_on_objects=0
#
# The name of the instantiation information file was explicitly specified
#
ii_file_specified=0
#
#  Temporary file used by command line processing
#
cmd_tmp_file=$eccp_tmpdir/cmd_tmp_file.txt
#
#  Temporary file used with a CPFE output filter.
#
output_tmp_file=$eccp_tmpdir/output_filter.txt
#
#  When --target is specified, contains the name of the target configuration.
#
target=
#
#  Flag that indicates that EDG_C_TO_OBJ_C99_OPTIONS should be added to
#  the command line.
#
need_c_to_obj_c99_options=0
#
#  For certain command-line options, no source file name is required (e.g. -v).
#
source_file_name_optional=0
#
# Define trap handlers
#
# Trap the "abort" signal to eliminate the shell-supplied diagnostic line
# that frequently includes the process number.
trap "trap_function 1" 1 # Hangup
trap "trap_function 1" 2 # Interrupt
trap "trap_function 1" 15 # Termination
trap "trap_function 134" 6 # abort
trap "trap_function 137" 9 # kill (used by timeout detection)
trap "trap_function 138" 10 # bus error
# segmentation fault (fails on some systems)
trap "trap_function 139" 11 2>/dev/null


eccp_exit()
#
# Remove any temporary files and exit.  The argument is the exit status.
#
{
  rm -rf $eccp_tmpdir
  exit $1
}


trap_function()
#
# Trap handler.  The first argument is the exit code.
#
{
  eccp_exit $1
}


#
# Function that takes a single argument, and returns that argument suitably
# quoted or escaped if necessary.  The escaped characters are then
# re-interpreted by the shell (in invoke_front_end) in order to pass arguments
# like '-DCLASS="ref class"' to the front end as a single argument.
#
shell_special='$&[](){}<>?^*!|;\\"`'
single_quote="'"
need_escape="${IFS}${shell_special}${single_quote}"
escape_if_needed()
{
  if [ "`echo $1 | tr -d "${need_escape}"`" = "$1" ] ; then
    # Nothing special.
    echo $1
  elif [ "`echo $1 | tr -d "${single_quote}"`" = "$1" ] ; then
    # No single quotes, so quote the entire argument.
    echo \'$1\'
  else
    # Has at least a single quote (so quotes won't work).  Escape all
    # lower case alpha characters (otherwise we might end up with, e.g., \n).
    echo $1 | sed 's/[^a-z]/\\&/g'
  fi
}  # escape_if_needed


#
# Function used to print driver debug information if driver debug is enabled.
#
try_debug_driver()
{
  if [ $driver_debug -ne 0 ] ; then
    if [ $verbose_driver_debug -ne 0 ] ; then
      printf "driver debug: "
    fi
    echo $1
  fi
}  # try_debug_driver


#
# Function used to print a driver error.
#
driver_error()
{
  if [ $verbose_driver_debug -ne 0 ] ; then
    # Emit a copy of the error with the "driver debug: " prefix for driver
    # debug tooling to easily pickup and expose the error.
    echo "driver debug: $driver_name: $1"
  fi
  echo "$driver_name: $1"
  error=1
}


#
# Function used to execute a grep operation on a fixed string.
#
grep_f()
{
  # Test for grep -F support. On older systems that do not provide grep -F,
  # attempt to fallback to fgrep.
  if echo "test" | grep -F "test" > /dev/null 2>&1 ; then
    grep -F $@
  else
    fgrep $@
  fi
}  # grep_f


#
# Function used to handle optional path mappings (e.g.,
#  "--foo files/abc.h=files/abc.h.ifc" or "--foo files/abc.h").
#
# The first argument is the option name (e.g., "--foo") and the second is the
# path mapping or path (e.g., "files/abc.h=files/abc.h.ifc" or "files/abc.h"
# respectively).
#
resolve_path_mapping()
{
  # Need to split on the '=' and convert the appropriate paths.
  arg1=`expr "$2" : '\(.*\)=.*'`
  arg2=`expr "$2" : '.*=\(.*\)'`
  if [ "${2#*=}" != "$2" ] ; then
    # This is a mapping of two different paths separated by an equals sign.
    if [ $1 != "--ms_mod_file_map" ] ; then
      arg1=$(native_path "$arg1")
    fi
    arg2=$(native_path "$arg2")
    echo "$arg1=$arg2"
  else
    # This is a single path.
    echo $(native_path "$2")
  fi
}  # resolve_path_mapping


#
# Function to report the module unit kind already being created (if any).
#
report_conflicting_module_unit_kind()
{
  case $module_unit_kind in
    1)
      echo "$driver_name: already creating a header unit."
      ;;
    2)
      echo "$driver_name: already creating a module interface."
      ;;
    3)
      echo "$driver_name: already creating an internal module partition."
      ;;
  esac
  eccp_exit 1
}  # report_conflicting_module_unit_kind


#
# Function called when an option that requires the creation of a header unit is
# specified.
#
mark_create_header_unit_specified()
{
  if [ $module_unit_kind -ne 0 ] ; then
    report_conflicting_module_unit_kind
    echo "$driver_name: cannot create an additional header unit."
    eccp_exit 1
  fi
  module_unit_kind=1
}  # mark_create_header_unit_specified


#
# Function called when an option that requires the creation of a module
# interface unit is specified.
#
mark_create_module_interface_specified()
{
  if [ $module_unit_kind -ne 0 ] ; then
    report_conflicting_module_unit_kind
    echo "$driver_name: cannot create an additional module interface."
    eccp_exit 1
  fi
  module_unit_kind=2
}  # mark_create_module_interface_specified


#
# Function called when an option that requires the creation of a module
# partition unit is specified.
#
mark_create_module_internal_partition_specified()
{
  if [ $module_unit_kind -ne 0 ] ; then
    report_conflicting_module_unit_kind
    echo "$driver_name: cannot create an additional internal module partition."
    eccp_exit 1
  fi
  module_unit_kind=3
}  # mark_create_module_internal_partition_specified


#
# Function called when collecting a module unit file.
#
collect_module_unit_src_file()
{
  arg=`native_path "$1"`
  # Add the file for building the header unit.
  if [ $any_module_unit_src_files -ne 0 ] ; then
    echo "$driver_name: cannot specify multiple module unit source files."
    eccp_exit 1
  fi
  module_unit_src_file="$arg"
  any_module_unit_src_files=1
}  # collect_module_unit_src_file


#
# Determine the file's base name.
#
basename_of_file()
{
  echo `expr //$1 : '.*/\(.*\)\.'`
}  # basename_of_file


#
# Function to return the name of the generated C file derived
# from a source file's base name.
#
derive_gen_c_file_name()
{
  basefile=$1
  if [ $keep_int_file -eq 1 -o $gen_c_in_curr_dir -eq 1 ] ; then
    echo "$basefile$gen_c_suffix"
  else
    echo "$eccp_tmpdir/$basefile$gen_c_suffix"
  fi
}  # derive_gen_c_file_name


#
# Function to return the display name of the generated C file derived from a
# source file's base name.
#
derive_gen_c_file_disp_name()
{
  basefile=$1
  echo "$basefile$gen_c_suffix"
}  # derive_gen_c_file_disp_name


#
# Function to return the name of the generated object file derived
# from a source file's base name.
#
derive_gen_obj_file_name()
{
  basefile=$1
  echo "$basefile$gen_o_suffix"
}  # derive_gen_obj_file_name


#
# Function that compiles a generated C file
#
# The first argument is the C file to compile (e.g., "$eccp_tmpdir/foo.int.c").
# The second argument is the name of the output file (e.g., "foo.o").  The
# third argument is a friendly file name for diagnostic purposes (e.g.,
# "foo.int.c").
#
compile_int_c()
{
#
# Remove #line directives if requested to do so.
#
  int_c_file=$1
  int_c_output=$2
  int_c_diag_name=$3
  add_obj_to_object_list=${4-1}
  cc_tmp_file=$eccp_tmpdir/c_output.txt
  if [ $strip_line_dirs -eq 1 ] ; then
    # Replace the #line directives with blank lines.  Also replace
    # GNU-style line directives with blank lines.
    sed -e "s/#line.*//" -e "s/# [0-9].*//" $int_c_file >$eccp_tmpdir/sld.txt
    mv -f $eccp_tmpdir/sld.txt $int_c_file
  fi
  command="$cc_command $c_to_obj_options -o "$int_c_output" -c $int_c_file"
  try_debug_driver "$command"
  $command >$cc_tmp_file 2>&1
  status=$?
  # MSVC echoes the filename back to the console.  Check for output that is
  # just the filename given back.
  unimportant_output=0
  int_c_basename=`basename $int_c_file`
  basename_size=`echo $int_c_basename | wc -c`
  tmp_file_size=`du -b $cc_tmp_file 2> /dev/null | cut -f1`
  if [ -z "$tmp_file_size" ]; then
    # Some error getting file size - make it a large number
    tmp_file_size=400000
  fi
  # Account for newlines
  if [ `expr $tmp_file_size - $basename_size` -lt 2 -a \
       "`cat $cc_tmp_file | tr -d \"\n\r\"`" = $int_c_basename ] ; then
    unimportant_output=1
  fi
#
# Display any diagnostics generated by the C compiler
#
  if [ -s $cc_tmp_file -a $suppress_c_to_object_diagnostics -eq 0 -a \
       $unimportant_output -eq 0 ] ; then
    echo $driver_name: diagnostics generated from compilation of $int_c_diag_name: >&2
    sed -e "s/eccp\.$eccp_tmp_pattern/eccptmp/g" $cc_tmp_file >&2
    echo $driver_name: end of diagnostics from compilation of $int_c_diag_name >&2
  fi
#
# If the C compiler returned a non-zero status, report that.
#
  if [ $status -ne 0 ] ; then
    echo $driver_name: $EDG_C_TO_OBJ_COMPILER compilation of $int_c_diag_name returned an exit status of $status
  fi
  rm -f $cc_tmp_file
  if [ $status -ne 0 ] ; then
    # Report underlying C compiler errors with an abort error status.
    status=129
  fi
  if [ $status -ne 0 ]
  then
    if [ $status -gt $max_status ]
    then
      max_status=$status
    fi
    any_errors=1
  else
#
#   Add the file to the list of .o files to be removed later.
#
    rofiles=$rofiles" "$int_c_output
    if [ $nm_on_objects -eq 1 ] ; then
      # Debug option that runs nm on generated object files
      nm $int_c_output | $EDG_DECODE
    fi
  fi
}  # compile_int_c #

#
# Function to expand an abbreviated command line option
#
check_abbreviation()
{
  arg_present=0
  keyword_option=0
  case $orig_arg in
    --)
      ;;
    --*=*)
      arg_present=1
      keyword_option=1
      opt_name=`expr $arg : '\(.*\)'=.*`    # Get the string before the =
      arg_value=`expr $arg : '.*=\(.*\)'`    # Get the string after the =
      ;;
    --*)
      keyword_option=1
      opt_name=$1
      ;;
  esac
  if [ $keyword_option -ne 0 ] ; then
    grep "^$opt_name" <<END_OF_INPUT >$cmd_tmp_file
--add_match_notes
--aligned_new
--alternative_tokens
--anachronisms
--arg_dep_lookup
--array_new_and_delete
--auto_instantiation
--auto_storage
--auto_type
--base_assign_op_is_default
--bit_precise_integers
--bool
--brief_diagnostics
--building_runtime
--c
--c11
--c17
--c18
--c23
--c23_typeof
--c89
--c99
--c++
--c++0x
--c++11
--c++14
--c++17
--c++20
--c++23
--c++26
--c++11_sfinae
--c++11_sfinae_ignore_access
--c++03
--c++cli
--c++cx
--c_to_obj_lib
--c_to_obj_option
--cfront_2.1
--cfront_3.0
--char8_t
--check_concatenations
--check_unicode_security
--clang
--clang_version
--class_name_injection
--clear_flag
--clr
--colors
--command
--comments
--compile
--compile_as_secondary_trans_unit
--compound_literals
--concepts
--const_string_literals
--constexpr_diag_error
--constexpr_diag_remark
--constexpr_diag_suppress
--constexpr_diag_warning
--context_limit
--cpfe_only
--cppcli
--cppcx
--create_pch
--create_header_unit
--create_module_interface
--create_module_internal_partition
--db
--db_alloc_seq
--db_name
--debug
--default_calling_convention
--default_common_tentative_definitions
--default_nocommon_tentative_definitions
--defer_parse_function_templates
--define_macro
--definition_list_file
--delegating_constructors
--dep_name
--dependencies
--deprecated_string_conv
--designators
--diag_error
--diag_once
--diag_remark
--diag_suppress
--diag_warning
--digit_separators
--dump_command_options
--dump_configuration
--dump_legacy_as_target
--display_error_number
--distinct_template_signatures
--dollar
--driver_debug
--early_tiebreaker
--embed_directory
--embedded_c
--embedded_c++
--enum_overloading
--error_limit
--error_output
--exc_spec_in_func_type
--exceptions
--explicit
--export
--extended_designators
--extended_variadic_macros
--extern_inline
--far_code_pointers
--far_data_pointers
--fixed_point
--force_vtbl
--friend_injection
--func_prototype_tags
--g++
--gcc
--gcc-be
--gcc_be
--gcc_gen_be
--gcc89_inlining
--gen_c_clang_version
--gen_c_gnu_version
--gen_c_msvc_version
--gen_move_operations
--gnu_version
--guiding_decls
--header_unit
--ignore_std
--il_display
--implicit_extern_c_type_conversion
--implicit_include
--implicit_noexcept
--implicit_typename
--include_directory
--incl_suffixes
--incognito
--inline_statement_limit
--inlining
--instantiate
--instantiation_dir
--keep_gen_c
--keep_restrict_in_signatures
--lambdas
--late_tiebreaker
--library_directory
--list
--list_macros
--list_object_files
--long_lifetime_temps
--long_long
--long_preserving_rules
--macro_positions_in_diagnostics
--max_cost_constexpr_call
--max_depth_constexpr_call
--microsoft
--microsoft_16
--microsoft_bugs
--microsoft_build_number
--microsoft_version
--mmap_address
--module_import_diagnostics
--module_init
--module_interface
--module_internal_partition
--modules
--modules_directory
--ms_await
--ms_await_strict
--ms_c++14
--ms_c++17
--ms_c++20
--ms_c++23
--ms_c++latest
--ms_c11
--ms_c17
--ms_c23
--ms_compatibility
--ms_cplusplus_std_value
--ms_extensions
--ms_header_unit
--ms_header_unit_angle
--ms_header_unit_quote
--ms_mod_file_map
--ms_permissive
--ms_rvalue_cast
--ms_std_preprocessor
--ms_stdc
--ms_strict_ternary
--ms_translate_include
--mscorlib_file_name
--msvc_target_version
--multibyte_chars
--multi_trans_unit
--munch
--named_address_spaces
--named_registers
--namespaces
--lossy_conversion_warning
--near_code_pointers
--near_data_pointers
--new_for_init
--nm
--no_add_match_notes
--no_aligned_new
--no_alternative_tokens
--no_anachronisms
--no_arg_dep_lookup
--no_array_new_and_delete
--no_auto_instantiation
--no_auto_storage
--no_auto_type
--no_base_assign_op_is_default
--no_bit_precise_integers
--no_bool
--no_brief_diagnostics
--no_c23_typeof
--no_c99
--no_c++0x
--no_c++11
--no_c++11_sfinae
--no_c++11_sfinae_ignore_access
--no_c++cli
--no_c++cx
--no_char8_t
--no_check_concatenations
--no_check_unicode_security
--no_clang
--no_class_name_injection
--no_code_gen
--no_colors
--no_compound_literals
--no_concepts
--no_const_string_literals
--no_cppcli
--no_cppcx
--no_defer_parse_function_templates
--no_definition_list_file
--no_delegating_constructors
--no_demangle
--no_dep_name
--no_deprecated_string_conv
--no_designators
--no_digit_separators
--no_display_error_number
--no_distinct_template_signatures
--no_embedded_c
--no_enum_overloading
--no_exc_spec_in_func_type
--no_exceptions
--no_explicit
--no_export
--no_extended_designators
--no_extended_variadic_macros
--no_extern_inline
--no_fixed_point
--no_friend_injection
--no_func_prototype_tags
--no_g++
--no_gcc
--no_gen_move_operations
--no_guiding_decls
--no_il_lowering
--no_implicit_extern_c_type_conversion
--no_implicit_include
--no_implicit_noexcept
--no_implicit_typename
--no_incognito
--no_inlining
--no_keep_restrict_in_signatures
--no_lambdas
--no_line_commands
--no_long_preserving_rules
--no_lossy_conversion_warning
--no_macro_positions_in_diagnostics
--no_microsoft
--no_microsoft_bugs
--no_module_import_diagnostics
--no_modules
--no_ms_compatibility
--no_ms_cplusplus_std_value
--no_ms_extensions
--no_ms_permissive
--no_ms_rvalue_cast
--no_ms_std_preprocessor
--no_ms_stdc
--no_ms_strict_ternary
--no_ms_translate_include
--no_multibyte_chars
--no_named_address_spaces
--no_named_registers
--no_namespaces
--no_nonconst_ref_anachronism
--no_nonstd_anonymous_unions
--no_nonstd_default_arg_deduction
--no_nonstd_gnu_keywords
--no_nonstd_instantiation_lookup
--no_nonstd_qualifier_deduction
--no_nonstd_using_decl
--no_nullptr
--no_old_id_chars
--no_old_specializations
--no_parse_templates
--no_pch_messages
--no_pch_verbose
--no_preproc_only
--no_preserve_lvalues_with_same_type_casts
--no_relaxed_abstract_checking
--no_remove_unneeded_entities
--no_restrict
--no_rtti
--no_rvalue_refs
--no_short_enums
--no_special_subscript_cost
--no_standard_includes
--no_std_libs
--no_stdarg_builtin
--no_stdc_zero_in_system_headers
--no_strict_gnu
--no_sun
--no_sun_linker_scope
--no_svr4
--no_template_typedefs_in_diagnostics
--no_thread_local_storage
--no_token_separators_in_pp_output
--no_trigraphs
--no_type_traits_helpers
--no_typename
--no_uliterals
--no_unrestricted_unions
--no_upc
--no_use_before_set_warnings
--no_user_defined_literals
--no_using_framework_directory
--no_using_std
--no_utf8_char_literals
--no_variadic_macros
--no_variadic_templates
--no_vla
--no_warnings
--no_wchar_t_keyword
--no_wrap_diagnostics
--nonconst_ref_anachronism
--nonstd_anonymous_unions
--nonstd_default_arg_deduction
--nonstd_gnu_keywords
--nonstd_instantiation_lookup
--nonstd_qualifier_deduction
--nonstd_using_decl
--nullptr
--old_c
--old_for_init
--old_id_chars
--old_ii_format
--old_line_commands
--old_specializations
--old_style_preprocessing
--one_instantiation_per_object
--optimize
--output
--output_mode
--pack_alignment
--parse_templates
--patch
--pch
--pch_dir
--pch_mem
--pch_messages
--pch_test_mode
--pch_verbose
--pending_instantiations
--pic
--preinclude
--preinclude_macros
--prelink_copy_if_nonlocal
--prelink_local_only
--prelink_objects
--preprocess
--preserve_lvalues_with_same_type_casts
--preusing
--promote_warnings
--purify
--quantify
--relaxed_abstract_checking
--remarks
--remove_instantiation_flags
--remove_unneeded_entities
--report_gnu_extensions
--restrict
--rtti
--rvalue_ctor_is_copy_ctor
--rvalue_ctor_is_not_copy_ctor
--rvalue_refs
--set_flag
--short_enums
--short_lifetime_temps
--signed_bit_fields
--signed_chars
--special_subscript_cost
--stdarg_builtin
--stdc_zero_in_system_headers
--strict
--strict_gnu
--strict_warnings
--stricter_template_checking
--strip
--strip_line_dirs
--sun
--sun_linker_scope
--suppress_c_to_obj_diagnostics
--suppress_instantiation_flags
--suppress_vtbl
--svr4
--sys_include
--target
--template_directory
--template_typedefs_in_diagnostics
--thread_local_storage
--time_limit
--timing
--top_templates
--trace_includes
--trans_unit_test_mode
--trigraphs
--type_traits_helpers
--typename
--uliterals
--undefine_macro
--unicode_source_kind
--unrestricted_unions
--unsigned_bit_fields
--unsigned_chars
--upc
--upc_relaxed
--upc_strict
--upc_threads
--use_pch
--user_defined_literals
--using_directory
--using_framework_directory
--using_std
--utf8_char_literals
--variadic_macros
--variadic_templates
--vcmeta_directory
--verbose_driver_debug
--version
--vla
--wchar_t_keyword
--wdir
--wrap_diagnostics
--xref
END_OF_INPUT
    opt_name_found=0
    new_opt_name=$opt_name
    for line in `cat $cmd_tmp_file`
    do
      if [ $opt_name_found -ne 0 ] ; then
        echo "$driver_name: more than one command line option matches the abbreviation $opt_name:"
        cat $cmd_tmp_file
        rm -f $cmd_tmp_file
        eccp_exit 1
      fi
      new_opt_name=$line
      opt_name_found=1
    done
    if [ $arg_present -ne 0 ] ; then
      arg="$new_opt_name=$arg_value"
    else
      arg=$new_opt_name
    fi
  fi
}  # check_abbreviation


process_option()
{
  add_to_instantiation_command=1
  used_two_params=0
  invalid_keyword_option=0
  curr_param=$2
  case $arg in
###############################################################################
# Options used by the driver
###############################################################################
    --)
#     Reported to be used sometimes by nmake -- simply ignore.
      ;;
    --driver_debug)
#     Show commands as they are executed.
      driver_debug=1
      ;;
    --verbose_driver_debug)
#     Show commands as they are executed prefixed with "driver debug: "
      driver_debug=1
      verbose_driver_debug=1
      ;;
    -O | --optimize)
#     Generate optimized code
      c_to_obj_options=$c_to_obj_options" -O";
      ;;
    -O*)
#     Generate optimized code (e.g., -O2)
      c_to_obj_options=$c_to_obj_options" $arg";
      ;;
    --optimize=*)
#     Generate optimized code (e.g., --optimize=2)
      arg_value=`expr $arg : '.*=\(.*\)'`    # Get the string after the =
      c_to_obj_options=$c_to_obj_options" -O$arg_value";
      ;;
    -S | --cpfe_only)
#     Run front end only.
      fe_only=1;
      keep_int_file=1;
      ;;
    --gcc-be | --gcc_be | --gcc_gen_be)
#     Use the native libgccjit backend directly.
      gcc_backend=1;
      suppress_patch_munch=1;
      feoptions=$feoptions" --gcc_gen_be";
      ;;
    -c | --compile)
#     Run front end and cc producing a .o file.
      cc_only=1;
      add_to_instantiation_command=0
      ;;
    -command | --command)
#     The command name to be used in the .ii file in place of what is
#     found in argument 0.
      compile_command=$curr_param
      used_two_params=1
      add_to_instantiation_command=0
      ;;
    -o | --output)
#     Explicitly name the executable.
      used_two_params=1
      executable=$curr_param
      executable=`native_path "$executable"`
      output_file_specified=1
      add_to_instantiation_command=0
      ;;
    -o*)
#     Explicitly name the executable.
      executable=`expr $arg : '-o\(.*\)'`    # Get the string after the -o
      executable=`native_path "$executable"`
      output_file_specified=1
      add_to_instantiation_command=0
      ;;
    --output=*)
#     Explicitly name the executable.
      executable=`expr $arg : '.*=\(.*\)'`    # Get the string after the =
      executable=`native_path "$executable"`
      output_file_specified=1
      add_to_instantiation_command=0
      ;;
    $library_option | --library_directory)
#     Collect a list of -L options to pass to the linker.
      Loptions=$Loptions" $library_option"$curr_param
      used_two_params=1
      ;;
    --c_to_obj_lib)
#     A library to be added to the EDG_C_TO_OBJ_LIBRARIES string
      EDG_C_TO_OBJ_LIBRARIES=$EDG_C_TO_OBJ_LIBRARIES" $lib_file_opt"$curr_param
      used_two_params=1
      add_to_instantiation_command=0
      ;;
    --c_to_obj_lib=*)
#     A library to be added to the EDG_C_TO_OBJ_LIBRARIES string
      arg_value=`expr $arg : '.*=\(.*\)'`    # Get the string after the =
      EDG_C_TO_OBJ_LIBRARIES=$EDG_C_TO_OBJ_LIBRARIES" $lib_file_opt"$arg_value
      add_to_instantiation_command=0
      ;;
    ${library_option}*)
#     Collect a list of -L options to pass to the linker.
      Loptions=$Loptions" "$arg
      ;;
    --library_directory=*)
#     Collect a list of -L options to pass to the linker.
      arg_value=`expr $arg : '.*=\(.*\)'`    # Get the string after the =
      Loptions=$Loptions" "$library_option$arg_value
      ;;
    -l*)
#     Collect a list of -l options to pass to the linker.
      arg_value=`expr $arg : '-l\(.*\)'`
      object_files=$object_files" "$lib_file_opt$arg_value
      any_l_or_o_files=1
      ;;
    -g*)
#     Generate debugging information
      if [ $arg = "-g" ] ; then
        arg=$EDG_C_TO_OBJ_COMPILER_DEBUG_FLAG
      fi
      c_to_obj_options="$c_to_obj_options $arg"
      ;;
    --debug)
#     Generate debugging information
      c_to_obj_options="$c_to_obj_options $EDG_C_TO_OBJ_COMPILER_DEBUG_FLAG"
      ;;
    --debug=*)
#     Generate debugging information
      arg_value=`expr $arg : '.*=\(.*\)'`    # Get the string after the =
      c_to_obj_options="$c_to_obj_options -g$arg_value"
      ;;
    -h | --no_standard_includes)
#     Suppress standard include directory.
      std_incl=0
      ;;
    -k | --keep_gen_c)
#     Keep generated C file
      keep_int_file=1;
      ;;
    -G)
#     Linker mode used to build shared libraries
      suppress_patch_munch=1
      ldoptions=$ldoptions" "$arg
      ;;
    --list_object_files)
#     Display a list of the object files.  This is used in
#     one instantiation per object mode to get an object list that includes
#     the instantiation object files.
      list_object_files=1
      suppress_link=1
      ;;
    -munch | --munch)
#     Use "munch" for handling static constructors and destructors
      patch_mode=0
      ;;
    --no_demangle)
#     Don't run the compiler output through edg_decode (instead use /bin/cat)
      EDG_DECODE=/bin/cat
      ;;
    --nm)
#     Run nm on the generated object files (used for debugging)
      nm_on_objects=1
      ;;
    --no_std_libs)
#     Don't include EDG_STD_LIBS in the link.
      EDG_STD_LIBS=
      ;;
    -patch | --patch)
#     Use "patch" for handling static constructors and destructors
      patch_mode=1
      ;;
    -pic | --pic)
#     Generate position independent code
      c_to_obj_options="$c_to_obj_options -pic"
      ;;
    -p | -pg)
#     Generate profiling code and use profiling version of libraries
      c_to_obj_options="$c_to_obj_options $arg"
      EDG_LIB_SUFFIX="_p"
      ;;
    -target)
#     SunOS 4.n option, as in "-target sun4" -- ignored.  See also --target.
      used_two_params=1
      ;;
    -Bstatic | -Bdynamic)
#     Pass through to linker in the object file list.  This is needed
#     because the position of these options is significant
      object_files=$object_files" "$arg
      ;;
    -purify | --purify)
#     Link using the purify command
      link_using_purify=1
      ;;
    --quantify)
#     Link using the quantify command
      link_using_quantify=1
      ;;
    -strip_line_dirs | --strip_line_dirs)
#     Remove #line directives from generated C
      strip_line_dirs=1
      ;;
    -suppress_c_to_obj_diagnostics | --suppress_c_to_obj_diagnostics)
#     Suppress diagnostics from the underlying C compiler
      suppress_c_to_object_diagnostics=1
      ;;
    --instantiation_dir)
#     The directory into which instantiation object files should be written.
#     Note that this has the same name as the option that is passed to the
#     front end, but that option controls where the .int.c files are
#     written, while this option controls where the .o files are written.
      instantiation_dir=$curr_param
      instantiation_dir=`native_path "$instantiation_dir"`
      use_default_instantiation_dir=0
      used_two_params=1
      ;;
    --instantiation_dir=*)
      arg_value=`expr $arg : '.*=\(.*\)'`    # Get the string after the =
      instantiation_dir=$arg_value
      instantiation_dir=`native_path "$instantiation_dir"`
      use_default_instantiation_dir=0
      ;;
    --multi_trans_unit)
#     When multiple files are specified, they should be treated as
#     multiple translation units to a single compilation
      multi_trans_unit=1
      ;;
    --no_definition_list_file)
#     Tell the prelinker not to use a definition list file.
      use_definition_list_file=0
      ;;
    --prelink_local_only)
#     Only files compiled in the current directory may have instantiations
#     assigned to them
      prelink_local_only=1
      ;;
    --prelink_copy_if_nonlocal)
#     If an assignment is done to a nonlocal object file, create a local
#     copy.
      prelink_copy_if_nonlocal=1
      ;;
    --prelink_objects)
#     Run the prelinker (but not the linker) on the object files.
      suppress_link=1
      ;;
    --remove_instantiation_flags)
#     Run the prelinker and request that it recompiles all of the objects
#     in such a way that the instantiation flags will be removed
      remove_instantiation_flags=1
      ;;
    --strip)
#     Run the "strip" command on the resulting executable
      strip_executable=1
      ;;
    -sun*)
#     SunOS 4.n option, as in "-sun4" -- ignored.
      ;;
    --pch_test_mode)
#     Special option for testing precompiled headers
      pch_test_mode=1
      ;;
    --trans_unit_test_mode)
#     Special option for testing multiple translation unit processing
      trans_unit_test_mode=1
      feoptions=$feoptions" $curr_arg"
      ;;
    --compile_as_secondary_trans_unit)
#     Special mode that compiles the specified file as a secondary file
      compile_as_secondary=1
      ;;
    --old_ii_format)
#     Use the old .ii file format that does not include the current directory
      old_ii_format=1
      ;;
    --c_to_obj_option)
#     Pass the argument that follows to the back end
      c_to_obj_options="$c_to_obj_options $curr_param"
      used_two_params=1
      ;;
###############################################################################
# Options passed to the front end that take no arguments
###############################################################################
    -a | --strict_warnings | \
    -b | --cfront_2.1 | \
         --cfront_3.0 | \
    -j | --no_use_before_set_warnings | \
    -m | --c | \
    -r | --remarks | \
    -s | --signed_chars | \
    -u | --unsigned_chars | \
    -v | --version | \
    -w | --no_warnings | \
    -W | --promote_warnings | \
    -x | --exceptions | \
         --no_exceptions | \
    -A | --strict | \
    -B | --implicit_include | \
         --no_implicit_include | \
    -C | --comments | \
    -K | --old_c | \
    -T | --auto_instantiation | \
         --no_auto_instantiation | \
    -V | --suppress_vtbl | \
         --anachronisms | \
         --no_anachronisms | \
    -# | --timing | \
         --c++ | \
         --c++0x | \
         --c++11 | \
         --c++14 | \
         --c++17 | \
         --c++20 | \
         --c++23 | \
         --c++26 | \
         --c89 | \
         --c99 | \
         --c11 | \
         --c18 | --c17 | \
         --c23 | \
         --no_c99 | \
         --no_c++0x | \
         --no_c++11 | \
         --c++03 | \
         --display_error_number | \
         --no_display_error_number | \
         --dollar | \
	 --old_line_commands | \
	 --microsoft | \
	 --no_microsoft | \
	 --microsoft_bugs | \
	 --no_microsoft_bugs | \
	 --microsoft_16 | \
         --modules | \
         --no_modules | \
         --module_interface | \
         --module_internal_partition | \
         --module_import_diagnostics | \
         --no_module_import_diagnostics | \
         --ms_await | \
         --ms_await_strict | \
         --ms_c++14 | \
         --ms_c++17 | \
         --ms_c++20 | \
         --ms_c++23 | \
         --ms_c++latest | \
         --ms_c11 | \
         --ms_c17 | \
         --ms_c23 | \
         --ms_compatibility | \
         --ms_stdc | \
         --no_ms_stdc | \
         --ms_std_preprocessor | \
         --no_ms_compatibility | \
         --no_ms_std_preprocessor | \
         --ms_cplusplus_std_value | \
         --no_ms_cplusplus_std_value | \
         --ms_extensions | \
         --no_ms_extensions | \
         --ms_permissive | \
         --no_ms_permissive | \
         --ms_rvalue_cast | \
         --no_ms_rvalue_cast | \
         --ms_strict_ternary | \
         --no_ms_strict_ternary | \
         --ms_translate_include | \
         --no_ms_translate_include | \
	 --cppcli | \
	 --no_cppcli | \
	 --c++cli | \
	 --no_c++cli | \
	 --clr | \
	 --cppcx | \
	 --no_cppcx | \
	 --c++cx | \
	 --no_c++cx | \
	 --far_data_pointers | \
	 --near_data_pointers | \
	 --far_code_pointers | \
	 --near_code_pointers | \
	 --long_lifetime_temps | \
	 --short_lifetime_temps | \
         --wchar_t_keyword | \
         --no_wchar_t_keyword | \
         --alternative_tokens | \
         --no_alternative_tokens | \
         --nonstd_gnu_keywords | \
         --no_nonstd_gnu_keywords | \
         --default_nocommon_tentative_definitions | \
         --default_common_tentative_definitions | \
         --inlining | \
         --no_inlining | \
         --gcc89_inlining | \
         --svr4 | \
         --no_svr4 | \
         --brief_diagnostics | \
         --no_brief_diagnostics | \
         --wrap_diagnostics | \
         --no_wrap_diagnostics | \
         --nonconst_ref_anachronism | \
         --no_nonconst_ref_anachronism | \
	 --no_preproc_only | \
         --rtti | \
         --no_rtti | \
         --building_runtime | \
         --bool | \
         --no_bool | \
         --array_new_and_delete | \
         --no_array_new_and_delete | \
         --explicit | \
         --no_explicit | \
         --namespaces | \
         --no_namespaces | \
         --using_std | \
         --no_using_std | \
         --restrict | \
         --no_restrict | \
         --special_subscript_cost | \
         --no_special_subscript_cost | \
         --remove_unneeded_entities | \
         --no_remove_unneeded_entities | \
         --type_traits_helpers | \
         --no_type_traits_helpers | \
         --typename | \
         --no_typename | \
         --implicit_typename | \
         --no_implicit_typename | \
         --suppress_instantiation_flags | \
         --old_style_preprocessing | \
         --old_for_init | \
         --new_for_init | \
         --old_specializations | \
         --no_old_specializations | \
         --implicit_extern_c_type_conversion | \
         --no_implicit_extern_c_type_conversion | \
         --long_preserving_rules | \
         --no_long_preserving_rules | \
         --extern_inline | \
         --no_extern_inline | \
         --guiding_decls | \
         --no_guiding_decls | \
         --vla | \
         --no_vla | \
         --distinct_template_signatures | \
         --no_distinct_template_signatures | \
         --multibyte_chars | \
         --no_multibyte_chars | \
         --enum_overloading | \
         --no_enum_overloading | \
         --embedded_c++ | \
         --nonstd_default_arg_deduction | \
         --no_nonstd_default_arg_deduction | \
         --nonstd_instantiation_lookup | \
         --no_nonstd_instantiation_lookup | \
         --nonstd_qualifier_deduction | \
         --no_nonstd_qualifier_deduction | \
         --nonstd_using_decl | \
         --no_nonstd_using_decl | \
         --one_instantiation_per_object | \
         --early_tiebreaker | \
         --late_tiebreaker | \
         --const_string_literals | \
         --no_const_string_literals | \
         --class_name_injection | \
         --no_class_name_injection | \
         --arg_dep_lookup | \
         --no_arg_dep_lookup | \
         --friend_injection | \
         --no_friend_injection | \
         --designators | \
         --no_designators | \
         --extended_designators | \
         --no_extended_designators | \
         --variadic_macros | \
         --no_variadic_macros | \
         --extended_variadic_macros | \
         --no_extended_variadic_macros | \
         --compound_literals | \
         --no_compound_literals | \
         --base_assign_op_is_default | \
         --no_base_assign_op_is_default | \
         --sun | \
         --no_sun | \
         --sun_linker_scope | \
         --no_sun_linker_scope | \
         --gcc | \
         --no_gcc | \
         --g++ | \
         --no_g++ | \
         --clang | \
         --no_clang | \
         --strict_gnu | \
         --no_strict_gnu | \
         --report_gnu_extensions | \
         --dep_name | \
         --no_dep_name | \
         --deprecated_string_conv | \
         --no_deprecated_string_conv | \
         --parse_templates | \
         --no_parse_templates | \
         --stricter_template_checking | \
         --export | \
         --no_export | \
         --stdarg_builtin | \
         --no_stdarg_builtin | \
         --stdc_zero_in_system_headers | \
         --no_stdc_zero_in_system_headers | \
         --ignore_std | \
         --il_display | \
	 --long_long | \
	 --upc | \
	 --no_upc | \
	 --upc_relaxed | \
	 --upc_strict | \
	 --short_enums | \
	 --no_short_enums | \
         --fixed_point | \
         --no_fixed_point | \
         --named_address_spaces | \
         --no_named_address_spaces | \
         --named_registers | \
         --no_named_registers | \
         --embedded_c | \
         --no_embedded_c | \
         --thread_local_storage | \
         --no_thread_local_storage | \
         --trigraphs | \
         --no_trigraphs | \
         --template_typedefs_in_diagnostics | \
         --no_template_typedefs_in_diagnostics | \
         --defer_parse_function_templates | \
         --no_defer_parse_function_templates | \
         --macro_positions_in_diagnostics | \
         --no_macro_positions_in_diagnostics | \
         --uliterals | \
         --no_uliterals | \
         --lambdas | \
         --no_lambdas | \
         --dump_configuration | \
         --dump_command_options | \
         --signed_bit_fields | \
         --unsigned_bit_fields | \
         --check_concatenations | \
         --no_check_concatenations | \
         --rvalue_refs | \
         --no_rvalue_refs | \
         --rvalue_ctor_is_copy_ctor | \
         --rvalue_ctor_is_not_copy_ctor | \
         --gen_move_operations | \
         --no_gen_move_operations | \
         --auto_type | \
         --no_auto_type | \
         --auto_storage | \
         --no_auto_storage | \
         --nullptr | \
         --no_nullptr | \
         --c23_typeof | \
         --no_c23_typeof | \
         --c++11_sfinae | \
         --no_c++11_sfinae | \
         --c++11_sfinae_ignore_access | \
         --no_c++11_sfinae_ignore_access | \
         --variadic_templates | \
         --no_variadic_templates | \
         --func_prototype_tags | \
         --no_func_prototype_tags | \
         --implicit_noexcept | \
         --no_implicit_noexcept | \
         --exc_spec_in_func_type | \
         --no_exc_spec_in_func_type | \
         --unrestricted_unions | \
         --no_unrestricted_unions | \
         --delegating_constructors | \
         --no_delegating_constructors | \
         --using_framework_directory | \
         --no_using_framework_directory | \
         --lossy_conversion_warning | \
         --no_lossy_conversion_warning | \
         --user_defined_literals | \
         --no_user_defined_literals | \
         --preserve_lvalues_with_same_type_casts | \
         --no_preserve_lvalues_with_same_type_casts | \
         --nonstd_anonymous_unions | \
         --no_nonstd_anonymous_unions | \
         --digit_separators | \
         --no_digit_separators | \
         --aligned_new | \
         --no_aligned_new | \
         --char8_t | \
         --no_char8_t | \
         --relaxed_abstract_checking | \
         --no_relaxed_abstract_checking | \
         --colors | \
         --no_colors | \
         --keep_restrict_in_signatures | \
         --no_keep_restrict_in_signatures | \
         --concepts | \
         --no_concepts | \
         --bit_precise_integers | \
         --no_bit_precise_integers | \
         --force_vtbl | \
         --utf8_char_literals | \
         --no_utf8_char_literals | \
         --check_unicode_security | \
         --no_check_unicode_security | \
         --old_id_chars | \
         --no_old_id_chars | \
         --add_match_notes | \
         --no_add_match_notes | \
         --incognito | \
         --no_incognito)
#     Options that require additional processing
      case $arg in
        -T | --auto_instantiation)
          automatic_instantiation=1
          ;;
        --no_auto_instantiation)
          automatic_instantiation=0
          ;;
        --building_runtime)
          building_runtime=1
          ;;
        -m | --c | --c89 | --c99 | --no_c99 | --c11 | --c18 | --c17 | --c23 | \
        --ms_c11 | --ms_c17 | --ms_c23 | -K | --old_c | --svr4 | --no_svr4 | \
        --gcc | --no_gcc | --upc | --no_upc)
          c_mode=1
          if [ $arg = "--c99" -o $arg = "--c11" -o $arg = "--c18" -o \
               $arg = "--c17" -o $arg = "--c23" -o $arg = "--ms_c11" -o \
               $arg = "--ms_c17" -o $arg = "--ms_c23" ] ; then
            need_c_to_obj_c99_options=1
          fi
          ;;
        -b | --c++ | \
	--c++03 | --c++0x | --no_c++0x | --c++11 | --no_c++11 | --c++14 | \
	--c++17 | --c++20 | --c++23 | --c++26 | --cfront_2.1 | --cfront_3.0 | \
	--g++ | --no_g++)
          c_mode=0
          ;;
	--no_preproc_only)
	  suppress_preproc_only=1
          ;;
        --embedded_c)
          # Link in the fixed-point runtime library, if any.
	  c_mode=1
          if [ "$EDG_FIXED_POINT_LIB" != "" ] ; then
            EDG_STD_LIBS="$EDG_STD_LIBS:$EDG_FIXED_POINT_LIB"
          fi
          ;;
        --module_interface)
          mark_create_module_interface_specified
          ;;
        --module_internal_partition)
          mark_create_module_internal_partition_specified
          ;;
        --suppress_instantiation_flags)
#         This should not be included in the command line in the .ii file.
          add_to_instantiation_command=0
          ;;
        --one_instantiation_per_object)
          one_instantiation_per_object=1
          ;;
        -v | --version | \
        --dump_configuration | \
        --dump_command_options | \
        --dump_legacy_as_target)
          # These options don't require a file name.
          source_file_name_optional=1
          ;;
      esac
      feoptions=$feoptions" $curr_arg"
      ;;
###############################################################################
# Options passed to the front end that take no arguments and must appear at
# the beginning of the option list passed to the front end.
###############################################################################
         --pch | \
         --pch_verbose | \
         --no_pch_verbose | \
         --pch_messages | \
         --no_pch_messages)
      feoptions=$curr_arg" $feoptions"
      ;;
###############################################################################
# Front end options with no arguments that suppress code generation.
###############################################################################
    -n | --no_code_gen | \
    -N | --no_il_lowering)
      feoptions=$feoptions" $curr_arg";
      fe_only=1
      ;;
###############################################################################
# Front end options with the argument specified in the following argument.
# For example, -d 5 or --db 5 look like two arguments to the shell.
###############################################################################
    -d | --db | \
    -e | --error_limit | \
    -i | --module_init | \
    -t | --instantiate | \
    -D | --define_macro | \
    -I | --include_directory | \
    -U | --undefine_macro | \
    -X | --xref | \
         --list | \
         --error_output | \
         --diag_suppress | \
         --diag_remark | \
         --diag_warning | \
         --diag_error | \
         --diag_once | \
         --constexpr_diag_suppress | \
         --constexpr_diag_remark | \
         --constexpr_diag_warning | \
         --constexpr_diag_error | \
         --embed_directory | \
         --header_unit | \
         --inline_statement_limit | \
         --max_cost_constexpr_call | \
         --max_depth_constexpr_call | \
         --microsoft_build_number | \
         --microsoft_version | \
         --mmap_address | \
         --modules_directory | \
         --ms_mod_file_map | \
         --ms_header_unit | \
         --ms_header_unit_angle | \
         --ms_header_unit_quote | \
         --gnu_version | \
         --clang_version | \
         --gen_c_clang_version | \
         --gen_c_gnu_version | \
         --gen_c_msvc_version | \
         --msvc_target_version | \
	 --definition_list_file | \
         --pending_instantiations | \
         --preinclude | \
         --preinclude_macros | \
         --sys_include | \
         --template_directory | \
         --time_limit | \
         --top_templates | \
         --incl_suffixes | \
         --db_alloc_seq | \
         --db_name | \
         --context_limit | \
         --set_flag | \
         --clear_flag | \
	 --upc_threads | \
         --pack_alignment | \
         --unicode_source_kind | \
         --mscorlib_file_name | \
         --preusing | \
         --using_directory | \
         --vcmeta_directory | \
         --default_calling_convention | \
         --dump_legacy_as_target | \
         --target | \
         --create_header_unit | \
         --create_module_interface | \
         --create_module_internal_partition | \
         --output_mode | \
         --wdir)
      used_two_params=1
#     See if an instantiation mode was specified
      case $arg in
        --definition_list_file)
          # The definition list file option is not put in the instantiation
          # command.
          add_to_instantiation_command=0
          ;;
        -t | --instantiate)
          instantiation_mode_specified=1
          ;;
        -I | --include_directory | --sys_include | --modules_directory | \
             --embed_directory)
          # Convert relative -I paths to absolute ones, if necessary.
          if [ $EDG_USE_ABSOLUTE_INCL_DIR_PATHS -eq 1 -a \
               "$curr_param" != "-" ] ; then
            absolute_path=`expr "$curr_param" : '/.*'`
            if [ $absolute_path -eq 0 ] ; then
              # The directory is a relative path.  Add the current directory
              # to convert it to an absolute path
              curr_param=$curr_dir/$curr_param
            fi
          fi
          curr_param=`native_path "$curr_param"`
          ;;
        --header_unit | \
        --ms_mod_file_map | \
        --ms_header_unit | \
        --ms_header_unit_angle | \
        --ms_header_unit_quote)
          curr_param=$(resolve_path_mapping "$arg" "$curr_param")
          ;;
        --target)
          # Capture the specified target configuration.
          target="$curr_param"
          ;;
        --create_header_unit)
          mark_create_header_unit_specified
          ;;
        --create_module_interface)
          mark_create_module_interface_specified
          ;;
        --create_module_internal_partition)
          mark_create_module_internal_partition_specified
          ;;
      esac
      feoptions=$feoptions" $curr_arg `escape_if_needed "$curr_param"`"
      ;;
###############################################################################
# Same as above, except these options must appear at the beginning of the
# command line passed to the front end.
###############################################################################
         --pch_mem | \
         --pch_dir | \
         --create_pch | \
         --use_pch)
      curr_param=`native_path "$curr_param"`
      feoptions=$curr_arg" `escape_if_needed "$curr_param"` $feoptions"
      used_two_params=1
     ;;
###############################################################################
# Front end options with the argument included as part of the option argument.
# For example, -d5 or --db=5 look like a single argument to the shell.
###############################################################################
    -d* | --db=* | \
    -e* | --error_limit=* | \
    -i* | --module_init=* | \
    -t* | --instantiate=* | \
    -D* | --define_macro=* | \
    -I* | --include_directory=* | \
    -U* | --undefine_macro=* | \
    -X* | --xref=* | \
          --list=* | \
          --embed_directory=* | \
          --error_output=* | \
          --diag_suppress=* | \
          --diag_remark=* | \
          --diag_warning=* | \
          --diag_error=* | \
          --diag_once=* | \
          --constexpr_diag_suppress=* | \
          --constexpr_diag_remark=* | \
          --constexpr_diag_warning=* | \
          --constexpr_diag_error=* | \
          --header_unit=* | \
          --inline_statement_limit=* | \
          --max_cost_constexpr_call=* | \
          --max_depth_constexpr_call=* | \
          --mmap_address=* | \
          --microsoft_build_number=* | \
          --microsoft_version=* | \
          --modules_directory=* | \
          --ms_mod_file_map=* | \
          --ms_header_unit=* | \
          --ms_header_unit_angle=* | \
          --ms_header_unit_quote=* | \
          --gnu_version=* | \
          --clang_version=* | \
          --gen_c_clang_version=* | \
          --gen_c_gnu_version=* | \
          --gen_c_msvc_version=* | \
          --msvc_target_version=* | \
          --pending_instantiations=* | \
          --preinclude=* | \
          --preinclude_macros=* | \
          --sys_include=* | \
          --template_directory=* | \
          --time_limit=* | \
          --top_templates=* | \
          --incl_suffixes=* | \
          --db_alloc_seq=* | \
          --db_name=* | \
          --context_limit=* | \
          --set_flag=* | \
          --clear_flag=* | \
	  --upc_threads=* | \
          --definition_list_file=* | \
          --pack_alignment=* | \
          --unicode_source_kind=* | \
          --mscorlib_file_name=* | \
          --preusing=* | \
          --using_directory=* | \
          --vcmeta_directory=* | \
          --default_calling_convention=* | \
          --dump_legacy_as_target=* | \
          --target=* | \
          --output_mode=* | \
          --wdir=* | \
          --create_header_unit=* | \
          --create_module_interface=* | \
          --create_module_internal_partition=*)
#     See if an instantiation mode was specified
      case $arg in
        --definition_list_file=*)
          # The definition list file option is not put in the instantiation
          # command.
          add_to_instantiation_command=0
          ;;
        -t* | --instantiate=*)
          instantiation_mode_specified=1
          ;;
        -I*)
          # Convert relative -I paths to absolute ones, if necessary.
          dir_name=`expr "$arg" : '-I\(.*\)'`    # Get the string after the -I
          if [ $EDG_USE_ABSOLUTE_INCL_DIR_PATHS -eq 1 ] ; then
            if [ "$dir_name" != "-" ] ; then
              absolute_path=`expr "$dir_name" : '/.*'`
              if [ $absolute_path -eq 0 ] ; then
                # The directory is a relative path.  Add the current directory
                # to convert it to an absolute path
                dir_name=$curr_dir/$dir_name
              fi
            fi
          fi
          curr_arg=-I`native_path "$dir_name"`
          ;;
        --sys_include=* | \
        --include_directory=* | \
        --modules_directory=* | \
        --embed_directory=*)
          # Convert relative --include_directory  paths to absolute ones,
          # if necessary.
          dir_name=`expr "$arg" : '.*=\(.*\)'`    # Get the string after the =
          opt_name=`expr "$arg" : '\(.*\)=.*'`    # Get the string before the =
          if [ $EDG_USE_ABSOLUTE_INCL_DIR_PATHS -eq 1 ] ; then
            if [ "$dir_name" != "-" ] ; then
              absolute_path=`expr "$dir_name" : '/.*'`
              if [ $absolute_path -eq 0 ] ; then
                # The directory is a relative path.  Add the current directory
                # to convert it to an absolute path
                dir_name=$curr_dir/$dir_name
              fi
            fi
          fi
          curr_arg=$opt_name=`native_path "$dir_name"`
          ;;
        --header_unit=* | \
        --ms_mod_file_map=* | \
        --ms_header_unit=* | \
        --ms_header_unit_angle=* | \
        --ms_header_unit_quote=*)
          # Need to split on the '=' and convert the appropriate paths.
          opt_arg=`expr "$arg" : '[^=]*=\(.*\)'`  # Get the string after the =
          opt_name=`expr "$arg" : '\([^=]*\)=.*'` # Get the string before the =
          opt_arg_mapped=$(resolve_path_mapping "$opt_name" "$opt_arg")
          curr_arg="$opt_name=$opt_arg_mapped"
          ;;
        --target=*)
          # Capture the specified target configuration.
          arg_value=`expr "$arg" : '.*=\(.*\)'`    # Get the string after the =
          target=$arg_value
          ;;
        --create_header_unit=*)
          mark_create_header_unit_specified
          ;;
        --create_module_interface=*)
          mark_create_module_interface_specified
          ;;
        --create_module_internal_partition=*)
          mark_create_module_internal_partition_specified
          ;;
      esac
      feoptions=$feoptions" `escape_if_needed "$curr_arg"`"
      ;;
###############################################################################
# Same as above, except these options must appear at the beginning of the
# command line passed to the front end.
###############################################################################
         --pch_mem=* | \
         --pch_dir=* | \
         --create_pch=* | \
         --use_pch=*)
      opt_value=`expr "$arg" : '.*=\(.*\)'`    # Get the string after the =
      opt_name=`expr "$arg" : '\(.*\)=.*'`    # Get the before the =
      opt_value=`native_path "$opt_value"`
      feoptions=$opt_name=`escape_if_needed "$opt_value"`" $feoptions"
      ;;
###############################################################################
# Preprocessing options
###############################################################################
    -E | --preprocess | \
    -H | --trace_includes | \
    -M | --dependencies | \
    -P | --no_line_commands | \
         --no_token_separators_in_pp_output | \
         --list_macros)
      preprocessor_only=1
      feoptions=$feoptions" $curr_arg";
      ;;
###############################################################################
# Unrecognized flag handling logic for the driver.
###############################################################################
    --*)
      # An invalid keyword option -- this is handled by the caller
      invalid_keyword_option=1
      ;;
    -*)
      driver_error "unknown option: $arg"
      add_to_instantiation_command=0
      ;;
###############################################################################
# Input file handling used by the driver.
#
# This logic is declared later to prevent interference with the processing
# of other command line options that reference an input file.
#
###############################################################################
    *\.a)
#     Collect a list of library archive (.a) files.
      object_files=$object_files" "$arg
      any_l_or_o_files=1
      add_to_instantiation_command=0
      ;;
    *\.so | *\.so\.*)
#     Collect a list of library shared object (.so) files.
      object_files=$object_files" "$arg
      any_l_or_o_files=1
      add_to_instantiation_command=0
      ;;
    *\.c | *\.C | *\.cc | *\.cpp | *\.CPP | *\.cxx | *\.CXX | *\.s)
#     Collect a list of .c files.
      arg=`native_path "$arg"`
      if [ "$cfiles" ]; then more_than_one_c_file=1; fi;
      if [ $multi_trans_unit -eq 0 -o $any_c_files -eq 0 ] ; then
        # In --multi_trans_unit mode only include the first object file
        # name in the list of object_files.
        # Get basename.o
        basename=$(basename_of_file "$arg")
        obj_file_name="$basename.o"
        object_files=$object_files" "$obj_file_name
      fi
      if [ $multi_trans_unit -ne 0 -a $any_c_files -ne 0 ] ; then
        # In --multi_trans_unit mode, this is a secondary file.  Add it to
        #  the list of secondary files.
        secondary_files=$secondary_files" "$arg;
      else
        # A primary source file, or not in --multi_trans_unit mode.
        cfiles=$cfiles" "$arg;
      fi
      any_c_files=1
      add_to_instantiation_command=0
      ;;
    *\.o)
#     Collect a list of .o files.
      arg=`native_path "$arg"`
      object_files=$object_files" "$arg
      any_l_or_o_files=1
      add_to_instantiation_command=0
      if [ ! -f $arg ] ; then
        driver_error "cannot open object file \"$arg\"."
      fi
      ;;
    *\.h | *\.H | *\.hpp | *\.HPP)
#     Collect a list of C++20 header unit files.
      collect_module_unit_src_file "$arg"
      module_unit_src_file_is_header=1
      ;;
    *\.ixx | *\.IXX | *\.cppm | *.CPPM)
#     Collect a list of normal C++20 module unit files.
      collect_module_unit_src_file "$arg"
      ;;
###############################################################################
# Fallback unrecognized argument handling logic for the driver.
###############################################################################
    *)
      driver_error "unrecognizable argument: $arg"
      ;;
  esac
  if [ $invalid_keyword_option -eq 0 -a $automatic_instantiation -eq 1 -a \
       $add_to_instantiation_command -eq 1 ] ; then
    # In automatic instantiation mode build a version of the command line
    # that can be used to compile one file.  This is mostly like the
    # original command without any file names and without certain linker
    # options.
    instantiation_command_line=$instantiation_command_line" "$curr_arg
    if [ $used_two_params -eq 1 ] ; then
      # The option took an argument -- append the argument.
      instantiation_command_line=$instantiation_command_line" "$curr_param
    fi
  fi
}  # process_option


#
# Function to invoke the front end and possibly redirect the error output
# to a particular place.  Use an echo/eval pair to execute the command to
# re-interpret any quoted arguments that may exist in the command.
#
invoke_front_end()
{
  try_debug_driver "$command"
  discard_output=$1
  if [ $discard_output -ne 0 ] ; then
    eval `echo $command` >/dev/null 2>&1
  elif [ "$EDG_CPFE_OUTPUT_FILTER" != "" ] ; then
    eval `echo $command` 2>$output_tmp_file
  else
    eval `echo $command`
  fi
  fe_status=$?
}  # invoke_front_end


#
# Function to check the exit code returned by the front end.
#
check_front_end_exit_code()
{
  #
  # If the front end aborted, report that.
  #
  if [ $fe_status -ge 128 ] ; then
    echo $driver_name: front end returned an exit status of $fe_status
    # EDG_SHOW_TRACEBACK enables a special debugging mode in which the
    # location of an abort is displayed.  This requires that a "show_traceback"
    # command exist and the CPFE be set to the full path of the executable.
    if [ ${EDG_SHOW_TRACEBACK-0} -gt 0 ] ; then
      show_traceback $CPFE
    fi
  fi
  if [ $fe_status -gt 1 ]
  then
    if [ $fe_status -gt $max_status ]
    then
      max_status=$fe_status
    fi
    any_errors=1
  fi
}  # check_front_end_exit_code


#
# Go through every argument, identify it, and add it to a list if appropriate.
#
while [ -n "$1" ]
do
  arg=$1
  orig_arg=$arg
  curr_arg=$arg
  process_option "$arg" "$2"
  if [ $invalid_keyword_option -ne 0 ] ; then
    check_abbreviation $arg
    process_option "$arg" "$2"
    if [ $invalid_keyword_option -ne 0 ] ; then
      driver_error "unknown option: $arg";
    fi
  fi
  shift;
  if [ $used_two_params -eq 1 ] ; then
    shift
  fi
done


#
# Print a command to create the temporary directory used by eccp.
#
try_debug_driver '# Recreate the temporary directory used by eccp'
try_debug_driver "mkdir -p \"$eccp_tmpdir\""


#
# Use target-specific variable values if --target has been specified or if
# there is a default target configuration.
#
if [ -z "$target" ] ; then
  # If no --target option has been specified, see if there is a default.
  if [ ! -z "$EDG_DEFAULT_TARGET" ] ; then
    target=$EDG_DEFAULT_TARGET
    feoptions=$feoptions" --target $target";
  fi
fi
if [ ! -z "$target" ] ; then
  LIBDIR="${LIBDIR}_$target"
  if [ ! -d $LIBDIR ] ; then
    echo "$driver_name: target-specific $LIBDIR does not exist"
  fi
  new_value=`eval echo \\$EDG_C_TO_OBJ_DEFAULT_OPTIONS_$target`
  if [ ! -z "$EDG_C_TO_OBJ_DEFAULT_OPTIONS" -a -z "$new_value" ] ; then
    echo "$driver_name: EDG_C_TO_OBJ_DEFAULT_OPTIONS_$target is unset in $config_file"
  else
    EDG_C_TO_OBJ_DEFAULT_OPTIONS=$new_value
  fi
fi

# Start building the command line.
cc_command="$EDG_C_TO_OBJ_COMPILER $EDG_C_TO_OBJ_DEFAULT_OPTIONS"
if [ $need_c_to_obj_c99_options -eq 1 -a \
     "$EDG_C_TO_OBJ_C99_OPTIONS" != "" ] ; then
  cc_command=$cc_command" "$EDG_C_TO_OBJ_C99_OPTIONS
fi

# Remove the temporary file used by command line processing.
rm -f $cmd_tmp_file

if [ $any_l_or_o_files -eq 0 -a $any_c_files -eq 0 ] ; then
  # For some class of command-line options, no source file is necessary
  # (but only run the front end in that case).
  if [ $any_module_unit_src_files -eq 1 ] ; then
    if [ $module_unit_kind -eq 1 ] ; then
      fe_only=1
    fi
  elif [ $source_file_name_optional -eq 1 ] ; then
    fe_only=1
  else
    driver_error "no source, object, or library files were specified"
  fi
fi

# Make sure the instantiation directory exists, if one was explicity
# specified.
if [ $use_default_instantiation_dir -eq 0 ] ; then
  if [ ! -d $instantiation_dir ] ; then
    driver_error "instantiation directory \"$instantiation_dir\" does not exist"
  fi
fi

# One instantiation per object mode requires driver versions >= 2.37
if [ $one_instantiation_per_object -ne 0 -a $driver_version -lt 237 ] ; then
  driver_error "one instantiation per object not supported in driver version $driver_version"
fi

# If we are in C mode then disable automatic instantiation just for
# efficiency.
if [ $c_mode -eq 1 ] ; then
  automatic_instantiation=0
fi

# The runtime library is built without automatic instantiation.  (Front ends
# configured without AUTOMATIC_TEMPLATE_INSTANTIATION do not accept the
# options the driver would otherwise pass to support it, e.g., --ii_file.)
if [ $building_runtime -eq 1 ] ; then
  automatic_instantiation=0
fi

# Put the command name on the beginning of the instantiation command line.
# This is done here because a different name can be supplied on the
# command line.  This is useful when the compile command is a script that
# sets some environment variables before invoking this script.
if [ $automatic_instantiation -eq 1 ] ; then
  instantiation_command_line="$compile_command -c"$instantiation_command_line
fi

if [ $error -eq 1 ]
then
  eccp_exit 1
fi

#
# Set the default include directories.
#
if [ $c_mode -eq 1 ] ; then
  default_include_dirs=$CINCLDIR
  if [ "$c_defines" != "" ] ; then
    feoptions=$c_defines" "$feoptions
  fi
else
  default_include_dirs=$INCLDIR
  if [ "$cpp_defines" != "" ] ; then
    feoptions=$cpp_defines" "$feoptions
  fi
fi
default_include_dirs=${EDG_DEFAULT_INCLUDE_DIRS-$default_include_dirs}
#
# Versions 2.42 and higher use the --sys_include for the default include
# directories.
#
include_option=--sys_include=
if [ $driver_version -lt 242 ] ; then
  include_option=-I
fi
#
# Add the -I before each element of a colon separated list.  Note that
# an empty list element is converted to a ".".
#
if [ "$default_include_dirs" != "" ] ; then
  raw_default_include_dirs=`echo $default_include_dirs | \
                            sed -e "s/::/:.:/g" \
                                -e "s/::/:.:/g" \
                                -e 's/:$//' -e s"/^:/.:/" \
                            | tr ':' '\n'`
  default_include_dirs=`echo "$raw_default_include_dirs" | \
                        while IFS= read include ; do
    include=\`echo "$include" | sed -e 's/^"//' -e 's/"$//'\`
    include=\`native_path "$include"\`
    echo "$include_option\"$include\" "
  done`
fi
#
# Add the proper include directory.
#
if [ $std_incl -eq 1 ]
then
    feoptions=$feoptions" "$default_include_dirs
fi
#
# Convert the default libraries into the appropriate form.  In
# other words, convert std:xyz to -lstd -lxyz.  Add an optional
# suffix.
#
if [ "$EDG_STD_LIBS" != "" ] ; then
  EDG_STD_LIBS=`echo $EDG_STD_LIBS | \
               sed -e 's/:$//' -e 's/::/:/g' \
                   -e 's/^:*//' -e "s/:/$EDG_LIB_SUFFIX $lib_file_opt/g"  \
                   -e "s/^/$lib_file_opt/" -e "s/$/$EDG_LIB_SUFFIX/"`
fi
if [ $suppress_preproc_only -eq 1 ] ; then
  # The --no_preproc_only option causes preprocessing only mode to be ignored.
  preprocessor_only=0
fi
if [ $preprocessor_only -eq 1 ] ; then
  # If we are only doing preprocessing, indicate that only the front end should
  # be run.
  fe_only=1
fi
#
# If only one source file was specified, and we are compiling and
# linking (i.e., we know everything for this compilation is in a single
# file) then use the "instantiate used" option.
#
if [ $c_mode -eq 0 -a $more_than_one_c_file -eq 0 -a $cc_only -eq 0 -a	\
     $EDG_NO_IMPLICIT_INSTANTIATE_USED -eq 0 -a \
     $fe_only -eq 0 -a $any_l_or_o_files -eq 0 -a \
     $instantiation_mode_specified -eq 0 ] ; then
  feoptions=$feoptions" -tused"
fi
#
# If the EDG_ONE_INSTANTIATION_PER_OBJECT flag is set, and we are not
# in C mode, enable the one instantiation per object option.
#
EDG_ONE_INSTANTIATION_PER_OBJECT=${EDG_ONE_INSTANTIATION_PER_OBJECT-0}
if [ $EDG_ONE_INSTANTIATION_PER_OBJECT -ne 0 -a $c_mode -eq 0 ] ; then
  one_instantiation_per_object=1
  feoptions=$feoptions" --one_instantiation_per_object"
fi
#
# Convert --prelink_local_only to the appropriate prelinker option.
#
if [ $one_instantiation_per_object -ne 0 ] ; then
  prelink_options=$prelink_options" -O"
fi
#
# Convert --no_definition_list_file to the appropriate prelinker option.
#
if [ $use_definition_list_file -eq 0 ] ; then
  prelink_options=$prelink_options" -a0"
fi
#
# The old .ii format cannot be used with the new prelinker nonlocal file
# options.
#
if [ $old_ii_format -ne 0 ] ; then
  if [ $prelink_local_only -ne 0 -o $prelink_copy_if_nonlocal -ne 0 ] ; then
    echo $driver_name: old .ii format may not be used with nonlocal prelinker options
    eccp_exit 1
  fi
fi
#
# Convert --prelink_local_only to the appropriate prelinker option.
#
if [ $prelink_local_only -ne 0 ] ; then
  prelink_options=$prelink_options" -D"
fi
#
# Convert --prelink_copy_if_nonlocal to the appropriate prelinker option.
#
if [ $prelink_copy_if_nonlocal -ne 0 ] ; then
  prelink_options=$prelink_options" -N"
fi
#
# Convert --list_object_files to the appropriate prelinker option.
#
if [ $list_object_files -ne 0 ] ; then
  prelink_options=$prelink_options" -b"
fi
#
# One instantiation per object and --prelink_copy_if_nonlocal require that
# a new object list file name be provided to the prelinker.
#
use_new_obj_list_file=0
if [ $prelink_copy_if_nonlocal -ne 0 -o $one_instantiation_per_object -ne 0 ]
then
  new_obj_list_file=$eccp_tmpdir/obj_list_file.txt
  use_new_obj_list_file=1
  prelink_options=$prelink_options" -o $new_obj_list_file"
fi
#
# Convert --remove_instantiation_flags to the appropriate prelinker option.
#
if [ $remove_instantiation_flags -ne 0 ] ; then
  prelink_options=$prelink_options" -S"
fi
#
# If we should use the old .ii file format, update the prelinker default
# options.
#
if [ $old_ii_format -ne 0 ] ; then
  prelink_options=$prelink_options" -R1"  
fi
#
# If we are using --multi_trans_unit mode, change cfiles so that it only
# contains the first file name.  Set all_files to the complete list.
#
if [ $multi_trans_unit -ne 0 ] ; then
  if [ $trans_unit_test_mode -ne 0 ] ; then
    echo "$driver_name: cannot combine --multi_trans_unit and --trans_unit_test modes."
    eccp_exit 1
  fi
  allfiles=$cfiles
  cfiles=`echo $cfiles | sed -e "s/ .*//"`
  more_than_one_c_file=0
fi
#
# If we are compiling the primary file as a secondary one, generate a
# dummy file to be used as the primary file.
#
if [ $compile_as_secondary -ne 0 ] ; then
  if [ "$EDG_DUMMY_PRIMARY_FILE" = "" ] ; then
    dummy_primary_file_name=$eccp_tmpdir/dummy_primary_file.c
    echo "extern int dummy_primary_filexxx;" >$dummy_primary_file_name
    if [ $? -ne 0 ] ; then
      echo $driver_name: could not create dummy primary file $dummy_primary_file_name.
      eccp_exit 1
    fi
    remove_dummy_primary=1
  else
    dummy_primary_file_name=$EDG_DUMMY_PRIMARY_FILE
  fi
fi
#
# Run the creation process for the given module file.
#
if [ $any_module_unit_src_files -eq 1 ] ; then
  if [ -n "$cfiles" ] ; then
    echo "$driver_name: cannot compile additional non-modules files when" \
         "compiling a module."
    eccp_exit
  fi
  if [ $module_unit_kind -eq 1 -a \
       $module_unit_src_file_is_header -eq 0 ] ; then
    echo "$driver_name: cannot compile a header unit from a non-header file."
    eccp_exit
  elif [ $module_unit_kind -ne 1 -a \
         $module_unit_src_file_is_header -eq 1 ] ; then
    echo "$driver_name: cannot compile a module unit from a header file."
    eccp_exit
  fi
  basefile=$(basename_of_file "$module_unit_src_file")
  if [ $module_unit_kind -eq 1 ] ; then
    # A header unit compilation is requested; compile the header.
    command="$CPFE $EDG_CPFE_DEFAULT_OPTIONS $feoptions $module_unit_src_file"
    invoke_front_end 0  # Run front end and keep output
    check_front_end_exit_code
  else
    # Otherwise, treat the module source file as a compiled source file and
    # run with linking disabled (i.e., cc_only=1).
    cc_only=1
    cfiles="$module_unit_src_file"
  fi
fi
#
# If there are no source files, invoke the front end with the options we
# have been given.
#
if [ -z "$cfiles" -a $source_file_name_optional -eq 1 ] ; then
  command="$CPFE $EDG_CPFE_DEFAULT_OPTIONS $feoptions"
  invoke_front_end 0  # Run front end and keep output
  check_front_end_exit_code
fi
#
# Run through the list of .c files and compile.
#
for cfile in $cfiles
do
  instantiation_command_suffix=
  basefile=$(basename_of_file "$cfile")
  suffix=`expr "$cfile" : '.*\.\(.*\)'` # Get the file suffix
  if [ $more_than_one_c_file -ne 0 ]
  then
    echo "$cfile:" 1>&2
  fi
  gen_c_file_name=$(derive_gen_c_file_name "$basefile")
  gen_c_file_disp_name=$(derive_gen_c_file_disp_name "$basefile")
  gen_c_obj_name=$(derive_gen_obj_file_name "$basefile")
  gen_c_option="--gen_c_file_name=$gen_c_file_name"
  if [ $cc_only -eq 1 -a $output_file_specified -eq 1 ] ; then
    # An output file name was specified using the -o option and
    # the -c option (compile only) is also in effect.  Take the
    # output file name as the name to be given to the first .o
    # file generated.
    output_file=$executable
    output_file_specified=0
    # The output file must be included in the options in the command line saved
    # in the .ii file for this object file.
    instantiation_command_suffix=$instantiation_command_suffix"-o $output_file"
    # Build the .ii file name based on the name of the object file being
    # built.
    output_basename=`expr "$output_file" : '\(.*\)\.'`  # Get basename
    ii_file_name=$output_basename.ii
    ii_file_specified=1
    ti_file_name=$output_basename.ti
  else
    output_file=$basefile.o
  fi
  gen_c_file_disp_name=$basefile$gen_c_suffix
  # Special handling for .s files
  if [ "$suffix" = "s" ] ; then
    try_debug_driver '# Compiling special s suffix case'
    compile_int_c "$cfile" "$output_file" "$gen_c_file_disp_name"
    continue
  fi
  # Build the name of the .ii file if it was not explicitly specified.  We
  # might end up specifying this even in cases where an ii file isn't
  # created (e.g., preprocessing only), but that won't hurt anything.
  ii_file_option=
  ti_file_option=
  if [ $automatic_instantiation -ne 0 ] ; then
    if [ $ii_file_specified -eq 0 ] ; then
      # No file name was specified -- construct the default name.
      ii_file_name=$basefile.ii
      ti_file_name=$basefile.ti
    fi
    if [ $ii_file_specified -eq 1 -o $compile_as_secondary -ne 0 ] ; then
      # A name was specified -- pass it to the front end.  Also do this when
      # using --compile_as_secondary mode.
      ii_file_option="--ii_file=$ii_file_name"
      if [ $driver_version -ge 237 ] ; then
        ti_file_option="--template_info_file=$ti_file_name"
      fi
    fi
  fi
  instantiation_dir_option=
  remove_instantiation_gen_c_dir=0
  # If we are using the one instantiation per object mode, pass in the
  # directory into which the instantiation .int.c files should be placed.
  # When we are keeping the .int.c files, they should go into the
  # instantiations directory, otherwise they go into a temporary directory.
  # When the "keep" option is used, the instantiations list goes into the
  # current directory, otherwise it goes in the temporary directory.
  if [ $one_instantiation_per_object -ne 0 ] ; then
    if [ $keep_int_file -ne 0 ] ; then
      instantiation_gen_c_dir=$instantiation_dir
      absolute_path=`expr "$instantiation_gen_c_dir" : '/.*'`
      if [ $absolute_path -eq 0 ] ; then
        # The directory is a relative path.  Add the current directory
        # to convert it to an absolute path
        instantiation_gen_c_dir=$curr_dir/$instantiation_gen_c_dir
      fi
    else
      instantiation_gen_c_dir=$eccp_tmpdir/instantiation.dir
      remove_instantiation_gen_c_dir=1
      # Attempt to remove any previously existing directory of this name.
      rm -rf $instantiation_gen_c_dir
      mkdir $instantiation_gen_c_dir
      if [ $? -ne 0 ] ; then
        echo "$driver_name: cannot create temporary directory $instantiation_gen_c_dir"
        eccp_exit 1
      fi
    fi
    instantiation_dir_option="--instantiation_dir=$instantiation_gen_c_dir"
    if [ $use_default_instantiation_dir -ne 0 -a \
         ! -d $instantiation_dir ] ; then
      mkdir $instantiation_dir
      if [ $? -ne 0 ] ; then
        echo "$driver_name: cannot create instantiation directory \"$instantiation_dir\""
        eccp_exit 1
      fi
    fi
  fi
  if [ $gcc_backend -ne 0 -a $preprocessor_only -eq 0 ] ; then
    if [ $fe_only -ne 0 ] ; then
      gen_c_option="-S -o $output_file"
    else
      gen_c_option="-c -o $output_file"
    fi
  fi
  command="$CPFE $EDG_CPFE_DEFAULT_OPTIONS $feoptions $gen_c_option $ii_file_option $ti_file_option $instantiation_dir_option"
  if [ $compile_as_secondary -ne 0 ] ; then
    # Append the dummy primary file name.
    command=$command" "$dummy_primary_file_name
  fi
  command=$command" "$cfile
  # Append the name of the file to be compiled
  # Normally we just append the file to be compiled, but in multi_trans_unit
  # mode we append the list of files.
  if [ $multi_trans_unit -ne 0 ] ; then
    command=$command" "$secondary_files
  fi
  if [ $trans_unit_test_mode -eq 1 -a $multi_trans_unit -eq 0 ] ; then
    # In translation unit test mode, specify the source file to be compiled
    # twice on the front end invocation command.
    command=$command" "$cfile
  fi
  # When using an output filter, direct the error output to a temporary
  # file.
  if [ "$EDG_CPFE_OUTPUT_FILTER" != "" ] ; then
    command_output="  2>$output_tmp_file"
  fi
  if [ $pch_test_mode -eq 1 ] ; then
    # In PCH test mode, we immediately repeat the same compilation.
    # The first compilation should generate a PCH file, the second should
    # use the generated file.  The output of the first compilation is
    # discarded.
    invoke_front_end 1  # Run front end and discard output
    invoke_front_end 0  # Run front end and keep output
    rm -f *.pch
  else
    # Normal mode, just run the front end.
    invoke_front_end 0  # Run front end and keep output
  fi
  if [ "$EDG_CPFE_OUTPUT_FILTER" != "" ] ; then
    # Run the output through the output filter.
    $EDG_CPFE_OUTPUT_FILTER $EDG_CPFE_OUTPUT_FILTER_OPTIONS <$output_tmp_file 1>&2
    rm -f $output_tmp_file
  fi
  #
  # Check the front end exit code.
  #
  check_front_end_exit_code
  #
  # Remove the dummy primary file, if any.
  #
  if [ $compile_as_secondary -ne 0 -a $remove_dummy_primary -ne 0 ] ; then
    rm -f $dummy_primary_file_name
  fi
  #
  # If we are doing automatic instantiation and if the program involves
  # templates then a .ii or .ti file will exist after the compilation,
  # depending on the driver version being used.
  # If a .ii or .ti file exists that means that the compilation used
  #  templates in some way.  The front end only generates the .ii and
  # .ti files when the back end is run (i.e., no "fe-only" options were
  #  specified and no errors occurred.
  #
  if [ $automatic_instantiation -ne 0 -a $preprocessor_only -eq 0 \
       -a $fe_only -eq 0 -a $fe_status -eq 0 ] ; then
    if [ $driver_version -ge 237 ] ; then
      # Create a new .ti file containing the driver-supplied information
      # followed by the information that was output by the front end.
      # The main reason this is done is that the instantiation directory
      # must come before any of the instantiation file name entries.
      if [ -f $ti_file_name ] ; then
        orig_file_content=$(cat "$ti_file_name")
        echo "cmd:$instantiation_command_line $instantiation_command_suffix" > "$ti_file_name"
        echo "dir:$curr_dir" >> "$ti_file_name"
        echo "fnm:$cfile" >> "$ti_file_name"
        if [ $one_instantiation_per_object -ne 0 ] ; then
          echo "idn:$instantiation_dir" >> "$ti_file_name"
        fi
        if [ $multi_trans_unit -ne 0 ] ; then
          echo "stu:$secondary_files" >> "$ti_file_name"
        fi
        printf "%s" "$orig_file_content" >> "$ti_file_name"
        if [ $driver_debug -ne 0 ] ; then
          # Create a temporary directory for the debug file.
          ti_dbg_file_dir=$(mktemp -d '/tmp/eccp-debug.XXXXXXXX')
          ti_file_basename="$(basename $ti_file_name)"
          ti_dbg_file="$ti_dbg_file_dir/$ti_file_basename"
          cp "$ti_file_name" "$ti_dbg_file"
          try_debug_driver "# Recreate eccp modifications of $ti_file_name"
          try_debug_driver "cat \"$ti_dbg_file\" > \"$ti_file_name\""
        fi
      fi
    else
      if [ -f $ii_file_name ] ; then
        # An instantiation file exists which means the compilation involves
        # templates.  Construct the new .ii file.
        ii_tmp_file=$eccp_tmpdir/temporary_ii.txt
        if [ $old_ii_format -ne 1 ] ; then
  #       New format
          sed -e "1,3 d" $ii_file_name >$ii_tmp_file
          echo $instantiation_command_line $instantiation_command_suffix >$ii_file_name
          pwd >>$ii_file_name
          echo $cfile >>$ii_file_name
        else
#         Old format
          sed -e "1,1 d" $ii_file_name >$ii_tmp_file
          echo $instantiation_command_line $instantiation_command_suffix $cfile >$ii_file_name
        fi
        cat $ii_tmp_file >>$ii_file_name
        rm -f $ii_tmp_file
        if [ $driver_debug -ne 0 ] ; then
          # Create a temporary directory for the debug file.
          ii_dbg_file_dir=$(mktemp -d '/tmp/eccp-debug.XXXXXXXX')
          ii_file_basename="$(basename $ii_file_name)"
          ii_dbg_file="$ii_dbg_file_dir/$ii_file_basename"
          cp "$ii_file_name" "$ii_dbg_file"
          try_debug_driver "# Recreate eccp modifications of $ii_file_name"
          try_debug_driver "cat \"$ii_dbg_file\" > \"$ii_file_name\""
        fi
      fi
    fi
  fi
#
# In one instantiation per object mode, extract the instantiation file
# list from the template information file.
#
  instantiation_list_exists=0
  if [ $automatic_instantiation -ne 0 ] ; then
    if [ $one_instantiation_per_object -ne 0 -a -f $ti_file_name ] ; then
      instantiation_list=$eccp_tmpdir/instantiation_list.txt
      grep_f "ifn:" $ti_file_name | sed -e "s/ifn://" >$instantiation_list
      instantiation_list_exists=1
    fi
  fi
#
#   Execute cc unless explicitly told not to.
#
  if [ $fe_status -eq 0 ] ; then
    if [ $fe_only -ne 1 -a $gcc_backend -eq 0 ]
    then
      # Compile the generate C file.
      try_debug_driver '# Compiling generated C file'
      compile_int_c "$gen_c_file_name" "$output_file" "$gen_c_file_disp_name"
    elif [ $fe_only -ne 1 -a $gcc_backend -ne 0 ]
    then
      rofiles=$rofiles" "$output_file
      if [ $nm_on_objects -eq 1 ] ; then
        nm $output_file | $EDG_DECODE
      fi
    fi
#
#   Compile the .int.c files for instantiation files that were generated
#
    if [ $instantiation_list_exists -ne 0 -a $fe_only -eq 0 ] ; then
      for inst_base in `cat $instantiation_list`
      do
        inst_file=$inst_base$gen_c_suffix
        inst_int_c=$instantiation_gen_c_dir/$inst_file
        inst_int_o=$inst_base$gen_o_suffix
        cd $instantiation_dir
        try_debug_driver '# Compiling generated instantiation file'
        compile_int_c "$inst_int_c" "$inst_int_o" "$inst_file"
        cd $curr_dir
      done
    fi
  fi
#
#     Remove the .int.c file.
#
  if [ $keep_int_file -eq 0 ]
  then
    rm -f $gen_c_file_name
  fi
#
#     Remove the .int.c files in the instantiation directory.
#
  if [ $instantiation_list_exists -ne 0 ] ; then
    if [ $keep_int_file -eq 0 ] ; then
      for inst_base in `cat $instantiation_list`
      do
        inst_file=$inst_base$gen_c_suffix
        rm -f $instantiation_gen_c_dir/$inst_file
      done
    fi
    # Remove the instantiation list temporary file
    rm -f $instantiation_list
  fi
#
# Remove the instantiation generated C directory, if needed.
#
  if [ $remove_instantiation_gen_c_dir -ne 0 ] ; then
    rm -rf $instantiation_gen_c_dir
  fi
done
if [ $any_errors -eq 0 ]
then
#  No compilation errors from front end or cc on any of the files.
  status=0
  if [ $fe_only -ne 1 ]
  then
    if [ $cc_only -ne 1 ]
    then
#
#     If automatic instantiation is enabled, run the prelink phase to
#     determine if any additional instantiations need to be generated
#     or if any existing instantiations are no longer needed.
#     If any instantiation list files are changed edg_prelink will
#     do the necessary recompilations.  When edg_prelink exits
#     the files will have been compiled and the necessary instantiations
#     generated.  Any instantiations that edg_prelink could not find
#     a way to generate will cause linker errors to be issued later.
#
      if [ $automatic_instantiation -ne 0 ] ; then
        command="$EDG_PRELINK $EDG_PRELINK_DEFAULT_OPTIONS \
                     $prelink_options \
		     $Loptions \
                     $EDG_DEFAULT_LIB_PATHS \
		     ${library_option}$LIBDIR \
                     $EDG_LINKER_LIB_PATHS \
		     $object_files -- \
                     $EDG_STD_LIBS"
        try_debug_driver "$command"
        eval $command
        status=$?
        if [ $status -gt $max_status ] ; then
          max_status=$status
        fi
      fi
#
#     When either one instantiation per object mode or the
#     --prelink_copy_if_nonlocal option is used, the prelinker outputs
#     an updated list of object files.  Replace the original ofiles list with
#     the updated one.
#
      if [ $use_new_obj_list_file -ne 0 ] ; then
        new_list=`cat $new_obj_list_file`
        rm -f $new_obj_list_file
        if [ $driver_debug -ne 0 ] ; then
          if [ "$object_files" != "$new_obj_list_file" ] ; then
            try_debug_driver 'Updating object file list'
            try_debug_driver "  old list: $object_files"
            try_debug_driver "  new list: $new_list"
          fi
        fi
        object_files="$new_list"
      fi
    fi
#
#   In --list_object_files mode, display the new object list.
#
    if [ $list_object_files -ne 0 ] ; then
      echo $object_files
    fi
#
#   Link the objects.  This is suppressed if we are just compiling, or
#   just prelinking the objects.
#
    if [ $cc_only -ne 1 -a $suppress_link -ne 1 ] ; then
#     Save the link command in a variable so it can be done again in the
#     "munch" step below.
#     Note:  -lC is missing from this command and is supplied later using
#     the variable link_command_suffix.
      link_command=${EDG_OBJ_TO_EXEC_LINKER-"$cc_command"}
      link_command_opts=${EDG_OBJ_TO_EXEC_DEFAULT_OPTIONS-"$c_to_obj_options"}
      link_command="$link_command $link_command_opts $Loptions \
		       $EDG_DEFAULT_LIB_PATHS \
		       ${library_option}$LIBDIR \
                       $ldoptions $output_flag$executable \
		       $EDG_STARTUP_FILE \
                       $object_files \
		       $EDG_STARTUP_FILE_2 \
                       $EDG_STD_LIBS \
		       $EDG_C_TO_OBJ_LIBRARIES"
      link_command_suffix=
      if [ ! -z $EDG_RUNTIME_LIB ]; then
        link_command_suffix=" $lib_file_opt$EDG_RUNTIME_LIB$EDG_LIB_SUFFIX"
      fi
      if [ $link_using_purify -eq 1 ] ; then
        link_command="purify $link_command"
      fi
      if [ $link_using_quantify -eq 1 ] ; then
        link_command="quantify $link_command"
      fi
#
#     Link the executable.  The linker output is saved to a file and then
#     fed through edg_decode to demangle the names.
#
      try_debug_driver "$link_command $link_command_suffix"
      link_error_file=$eccp_tmpdir/link_error_file.txt
      $link_command $link_command_suffix >$link_error_file 2>&1
      status=$?
      $EDG_DECODE <$link_error_file 1>&2
      if [ $status -gt $max_status ] ; then
        max_status=$status
      fi
      if [ $status = 0 -a $c_mode -eq 0 ]
      then
#       Do processing to handle calling static constructors and destructors.
        if [ $suppress_patch_munch -eq 1 ] ; then
#         Skip the patch/munch phase
          dummy=1   # Shell does not like an empty if
        elif [ $patch_mode = 1 ] ; then
#         Do "patch" processing.
          chmod -x $executable
          command="$PATCH $executable"
          try_debug_driver "$command"
          $command
          status=$?
          if [ $status -gt $max_status ] ; then
            max_status=$status
          fi
          if [ $status = 0 ]
          then
            chmod +x $executable
          fi
        else
#
#         Do "munch" processing:
#            1. Run munch on executable to produce C file
#            2. Compile C file
#            3. Re-link with object of C file
#
          tmpfile=$eccp_tmpdir/munch_tmp
          command="nm $EDG_MUNCH_NM_OPTIONS $executable | \
                   $MUNCH $EDG_MUNCH_OPTIONS"
          try_debug_driver "$command"
          eval $command >$tmpfile.c
          command="$cc_command $c_to_obj_options -c $tmpfile.c"
          try_debug_driver "$command"
          (cd $eccp_tmpdir; $command)
          status=$?
          if [ $status -ne 0 ] ; then
            echo "$driver_name: compilation of file generated by munch failed"
            eccp_exit $status
          fi
#         Do the link again.
          command="$link_command $tmpfile.o $link_command_suffix"
          try_debug_driver "$command"
          $command >$link_error_file 2>&1
          status=$?
          $EDG_DECODE <$link_error_file 1>&2
          if [ $status -gt $max_status ] ; then
            max_status=$status
          fi
          rm -f $tmpfile.c $tmpfile.o
        fi
      fi
      if [ $strip_executable -ne 0 ] ; then
#       Run the "strip" command on the executable if requested.
        command="$STRIP $executable"
        try_debug_driver "$command"
	$command
      fi
      if [ "$rofiles" != "" ] ; then
        rm -f $rofiles
      fi
      rm -f $link_error_file
    fi
  fi
fi
eccp_exit $max_status
