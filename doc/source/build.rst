==========================================
Configuring and Building the C++ Front End
==========================================



This chapter is an introduction to configuring and building the front end and
the various tools that come with it.  Bringing up the front end on a new
platform typically can be done in a few hours, and if the platform is close
enough to a system for which sample configurations are available (e.g., Linux,
Windows, etc.) that initial process can be reduced to a matter of minutes.

The front end can be configured for a variety of uses.  For example, it could
be part of a compiler with a custom code generator, or a compiler with a
portable C-generating back end (the latter is provided).  Alternatively, it
could be part of a source analysis tool (without a code generator) or of a
source transformation/translation tool (using the supplied C/C++-generating
back end).  Some of the tools supplied with the front end will not be needed
for some of these applications.  For example, a source analysis tool is
unlikely to need the supplied prelinker tool to perform automatic
instantiations.  The details of the build process are therefore likely going to
depend on the application.  However, the overall structure of the source code
is simple enough that customizing the build process should be straightforward.

The source code implementing the front end and associated tools is written in
C++11 (i.e., ISO/IEC 14882:2011) and can be compiled using relatively current
C++ compilers.  The front end has been compiled with g++ 4.8.1, clang 3.3, and
Microsoft Visual Studio 2015 (build 19.00.24215.1).  The front end does not
explicitly throw or catch exceptions and does not use C++ features that would
requite runtime type information (RTTI), so those features can be disabled when
compiling the front end (for compilers that support that).  (The sample
run-time support library is written in C++ and (if needed) is normally compiled
using the front end itself.  When building on Microsoft Windows, a component is
also available to read metadata in C++/CLI mode: This is written in C++ for the
Microsoft C++ compiler.)

Directories and Files in a Release
==================================

After unpacking a release, its top-level directory should contain the following
directories and files:

.. list-table::

   * - | ``Makefile``
     - | In a Unix-like environment (including Windows-hosted environments like
         Cygwin or MinGW), the ``make`` utility can be used to build the front
         end and its supporting tools and components.  This top-level
         ``Makefile`` runs the ``make`` tool in the various subdirectories as
         needed.  It is geared primarily toward building a compiler using the
         C-generating back end along with the sample run-time support library.
         A ``make`` process using this top-level ``Makefile`` may therefore
         terminate with errors for other situations; in that case, the
         ``Makefile``\ s in the appropriate subdirectories (especially
         ``misc/`` and ``src/``) are still likely to be useful.
   * - | ``src/``
     - | The directory containing the source files of the C++ front end and
         its optional components (such as the C- and C++-generating back
         ends).  In addition to source code, the ``src/`` directory also
         holds the ``Changes`` file, which describes the various changes
         made to the front end in the current and previous releases.  Note
         that most of the source files have the ``.c`` suffix that would
         normally indicate a C source file.  Starting with the 6.0 release,
         these files contain C++ source code and must be compiled using a
         C++ compiler (with the C++14 dialect).  The original filenames
         have been maintained to make patching files easier.  If desired,
         symbolic links can be added to accommodate build environments as
         necessary.
   * - | ``bin/``
     - | Initially, this directory contains a script ``eccp`` that can be used
         to run the front end much like typical C++ compilers in Unix-like
         environments (such as ``CC`` and ``g++``).  See
         ":ref:`the-eccp-script`" for details.  Once the front end is built (in
         the ``src/`` directory), it can be moved here for invocation by
         ``eccp`` (this is done automatically if the top-level ``Makefile`` is
         used).
   * - | ``sample_edg_eccp_config/``
     - | The ``eccp`` script can be configured in various ways.  This directory
         contains sample configuration files for some popular Unix-like
         operating systems (Linux, MacOS X, and Solaris).  It also includes a
         script ``make_g++_incl_paths`` (to be run from the ``lib/`` directory)
         to establish the location of ``g++``\ 's header files (in cases where
         ``eccp`` should rely on a native ``g++`` installation for system and
         standard header files).
   * - | ``include/``
     - | This directory contains prototype implementations for a few standard
         C++ header files that are closely tied to the compiler's internals.
         (For example, the contents of the ``<typeinfo>`` header must match the
         compiler's understanding of the ``std::type_info`` type.)
   * - | ``lib_src/``
     - | This directory contains C++ source code implementing a minimal C++
         run-time support library (in support of C++ features like exception
         handling, the ``dynamic_cast`` operator, and ``operator new``).  The
         library also includes support for certain extensions to the C89
         language (like operations on complex floating-point types and support
         for variable-length arrays).  All of this code is primarily meant for
         demonstration purposes, and is probably not suitable for end-user
         products: It is very portable, but not very efficient.  It is usually
         not needed for non-compiler applications such as source analysis or
         source-to-source translation.  This directory also contains a
         ``Changes`` file describing the history of the run-time support
         library.
   * - | ``lib/``
     - | Initially, this directory only contains a file
         ``predefined_macros.txt``.  This file is read by the front end to
         predefine certain macros.  It is typically used to define macros
         that are defined by compilers being emulated (see
         ``make_predef_macro_table`` and ``make_win_predef_macro_table.c``
         for utilities that automatically generate a set of predefined
         macros for GNU and clang and Microsoft compilers, respectively).
         Once the run-time support library is built (in the ``lib_src/``
         directory), it can be moved here so the ``eccp`` script can find
         it (this is done automatically if the top-level ``Makefile`` is
         used).  Note that this directory is only used when an unnamed
         "legacy" target configuration is specified -- see ``lib_``\
         *target*\ ``/`` below for cases where a named target configuration
         is used.
   * - | ``lib_``\ *target*\ ``/``
     - | Target-specific version of the ``lib/`` directory described above
         used when the "``--target`` *target*" command-line option is
         specified.  This directory can have the same
         ``predefined_macros.txt`` and ``libC.a`` files as ``lib/``, or can
         be different as required by the specific target.  If a
         target-specific version of ``libC.a`` is required, it must be
         built manually (see ``lib_src/Makefile``).
   * - | ``misc/``
     - This directory contains sources for four utility programs (and a
       ``Makefile`` to build them) to build and configure the front end:

       * ``make_predef_macro_table``: A script to generate a predefined
         macros file (like ``lib/predefined_macros.txt``) to match given
         versions of ``gcc/g++`` and ``clang``.
       * ``make_win_predef_macro_table.c``: A program to generate a
         predefined macros file (like ``lib/predefined_macros.txt``) to match
         a given version of Microsoft Visual Studio.  Note that this program
         is not built by the ``Makefile`` (and must be built manually if
         required).
       * ``mk_errinfo.c``: A program to translate the files describing the
         diagnostics emitted by the front end to plain C++ code.
       * ``dettarg.c``: A utility to determine basic configuration settings for
         a target platform (e.g., the size and alignment of various integer
         types).  See ":ref:`the-detarg-utility`" below.
   * - | ``util/``
     - This directory contains sources for three utility programs (and a
       ``Makefile`` to build them) that can be used in conjunction with the
       front end (e.g., by the ``eccp`` script):

       * ``edg_prelink.c``: A program that implements automatic template
         instantiation by re-running the front end to generate missing
         instantiations.
       * ``edg_decode.c``: A name demangler (which can, for example, be used on
         the output of a linker to make that output more readable). It is
         an interface for the actual demangling logic, which is contained
         in ``decode.c`` to allow its use in other programs as well.
       * ``edg_munch.c``: A utility that generates a C program that will call
         all required start-up initialization routines.  This is needed only
         if your linker does not provide a way to invoke code automatically at
         program start-up time.
   * - | ``doc/``
     - | This directory contains a PDF version of this document (``plm.pdf``).
   * - | ``README``
     - | A plain text file summarizing some of the information in this chapter.

Configuring the Front End
=========================

A large number of behaviors of the front end can be configured by appropriately
defining various configuration macros.  These macros can affect which
components of the front end are enabled, which dialects are supported, what
host and target platform characteristics are assumed, and so forth.  See
:ref:`macro-defaults` below for information about where all the macros are
documented.

Beginning with version 4.10.1 of the front end, a facility to provide for
run-time selection of a target configuration (through the use of the
``--target`` command-line option) is provided.  To support this run-time
target-configuration feature, a number of the configuration macros have
been designated as "target-specific".  See
:ref:`target-specific-configuration` for more information.

The ``defines.h`` File
----------------------

It is highly recommended that the front end be configured by defining the
appropriate macros in the ``src/defines.h`` file.  Occasionally, it may be
expedient to set the macros directly through ``src/Makefile`` or a similar
build-system mechanism (e.g., to enable debugging facilities), but modifying
other front end sources to set the configuration macros is not a recommended
option.

Sample configuration files are provided in the ``src/`` directory for Linux,
MacOS X, Solaris, and Windows (``defines.h.linux``, ``defines.h.macosx``,
``defines.h.solaris``, and ``defines.h.win32``, respectively).  These are
recommended starting points when building the front end on these platforms: For
example, to build the front end on MacOS X, one can start by copying
``src/defines.h.macosx`` to ``src/defines.h``.

A partial configuration file ``src/defines.h.proto`` is also provided: It
contains the macros needed to enable the modern IA-64 ABI (also called the
Itanium ABI), but it must be complemented by additional target characteristics
(which can be obtained by running the ``dettarg`` utility described next).

.. _the-detarg-utility:

The ``dettarg`` Utility
-----------------------

File ``misc/dettarg.c`` is a simple C++ program that can be compiled, linked,
and run on a system for which the compiler is targeted to determine some basic
characteristics (like endianness, size and alignment of fundamental types, and
so forth) of that target platform.  The output produced by ``dettarg`` is in a
form suitable for addition to the ``src/defines.h`` file.

.. _macro-defaults:

``lang_feat.h``, ``host_envir.h``, ``targ_def.h``, and ``target.h``
-------------------------------------------------------------------

A typical ``src/defines.h`` file will not explicitly set all the configuration
macros available in the front end.  Instead, most macros are just left to their
default values.  These defaults are specified in four header files.  Perhaps
more importantly, these header files are also the place where all the
configuration macros are documented (in comments preceding the directives
determining the default values).  The header files are as follows:

.. list-table::

   * - | ``src/lang_feat.h``
     - | Configuration macros controlling language features.  Some macros
         control a single language feature detail, whereas others control whole
         dialects.
   * - | ``src/host_envir.h``
     - | Configuration macros that determine characteristics of the host
         platform (the platform on which the front end runs).
   * - | ``src/targ_def.h``
       | ``src/target.h``
     - | Configuration macros describing characteristics of the target platform
         (the platform for which code must be generated).  Many of these macros
         have names that begin with ``TARG_``.  Characteristics that are
         entirely fixed at the time the front end is built are normally
         documented in ``targ_def.h``, whereas ``target.h`` describes target
         characteristics that can be overridden for every invocation of the
         front end.

These descriptions are generally correct, but occasionally a configuration
macro may need to be placed in a header that doesn't quite match that macro's
function.  For example, ``targ_def.h`` contains some macros that would more
naturally belong to ``lang_feat.h``, but the default value of the macro depends
on a target platform characteristic.

Note that these files should not be modified to set configuration options.
Instead, the appropriate macros should be defined in ``src/defines.h`` or
perhaps in a ``Makefile`` or similar build system tool.

``basics.h`` and Build Environment Configuration
------------------------------------------------

The front end is written in portable C++11 (except for one file in the
Microsoft dialect if the capability of reading Microsoft metadata for
C++/CLI is needed).  The ``basics.h`` header adds abstractions to allow
most of the remainder of the front end source code to be written without
concern for variations in system headers.  The build environment type can
often be determined by the preprocessor tests in ``basics.h``, but when
that is not the case, one should define one of the following build
environment macros (on the command-line or in ``src/defines.h``):
``__ANSIC__`` (for ANSI C; also used for Borland and Microsoft C under
MS-DOS and Windows), ``__SYSV__`` (for System V), ``__BSD__`` (for Berkeley
4.n), ``__VMS__`` (for VAX/VMS C; this will cause ``__SYSV__`` to be
defined, but the ``__VMS__`` flag will control some minor variations from
System V).

Some ANSI library functions do not exist in all system libraries.  This problem
is handled by remapping such function calls (via macros) to functions that do
exist.  For example, traditional Berkeley UNIX does not have ``memcpy``, but
does have ``bcopy``; the front end source is written using calls to ``memcpy``,
but when it is compiled with the ``__BSD__`` flag set, a macro ``memcpy``
transforms those calls into calls to ``bcopy``.  Another function of the build
environment macros is controlling inclusion of appropriate header files.  Even
when identical functions exist on different host systems, they are sometimes
defined by different header files (e.g., Berkeley systems use ``<strings.h>``
instead of the ANSI/System V ``<string.h>``).  ``basics.h`` includes the
appropriate definitions for the most common functions (standard I/O, string and
block operations, and character typing); any other required headers are
included only in those source files that need them, with appropriate
conditional compilation surrounding the ``#include``\ s.

Very occasionally, the macro definitions in ``basics.h`` must be modified
directly (i.e., by editing ``basics.h``).  Specifically, when ``__ANSIC__`` is
not set, some ``#define``\ s in ``basics.h`` must establish values for about a
dozen of the most useful values (specifically, ``CHAR_BIT``, ``CHAR_MIN``,
``CHAR_MAX``, ``UCHAR_MAX``, ``SHRT_MAX``, ``INT_MAX``, ``LONG_MAX``,
``LONG_MIN``, and ``ULONG_MAX``).  These should be set to proper values for the
host.  ``sizeof_t`` should also be defined as the integral type to be used for
sizes of things.  Usually this is the same as ``size_t``, but it might be
different if ``size_t`` is too small (e.g., 16 bits).  These values cannot be
set in ``defines.h`` or on the compilation command line.

Finally, ``basics.h`` also determines the default values of some macros that
control the presence of code aiding in the development of the front end (e.g.,
``DEBUG`` for debugging aids and ``CHECKING`` for internal consistency checks).

The front end requires a minimal C++ standard library.  Prior to release 6.0,
the front end was written in C and used various host C standard library
functions (e.g., for file I/O).  When switching to C++, it was decided not to
use the additional portions of the C++ standard library that became available
(e.g., ``std::string``) as this would add a burden to our customers and would
require testing against many versions of the C++ standard library.  Instead, a
small subset of library-like utility functions was developed for the front end
(in ``util.h``).  As a result, the portion of a C++ standard library that the
front end requires is about the same as that provided by a C standard library.

Commonly-used Configuration Macros
----------------------------------

The following subsections describe a selection of macros that commonly appear
in ``src/defines.h``.  This list is not meant to be exhaustive.  Many (but not
all) configuration macros are used to initialize global variables with a
similar name, and it is the value of the variable that controls a particular
behavior.  For example, ``GCC_IS_GENERATED_CODE_TARGET`` (described below)
initializes the global variable ``gcc_is_generated_code_target``.  This allows
particular behaviors to be controlled at run time (via the command line or via
custom code additions).

Note that none of the configuration macros described below are
"target-specific" (so, for example, there can't be one target configuration
where ``DO_IL_LOWERING`` is TRUE and another where it is FALSE).

Optional Components
^^^^^^^^^^^^^^^^^^^

.. list-table::

   * - | ``BACK_END_SHOULD_BE_CALLED``
     - | TRUE if a back end should be called (the call is to a function
         ``back_end``).
   * - | ``DO_IL_LOWERING``
     - | TRUE if IL lowering (rewriting C++-specific constructs in the
         intermediate language in C form) should be done.
   * - | ``BACK_END_IS_C_GEN_BE``
     - | TRUE if the C-generating back end should be used.  If FALSE, some
         other back end must be supplied or ``BACK_END_SHOULD_BE_CALLED`` must
         be FALSE.  The C-generating back end takes the lowered IL for a
         translation unit and generates a C source file suitable for
         compilation to binary code by a native C compiler.
   * - | ``BACK_END_IS_CP_GEN_BE``
     - | TRUE if the C++/C-generating back end should be used.  If FALSE, some
         other back end must be supplied or ``BACK_END_SHOULD_BE_CALLED`` must
         be FALSE.  The C++/C-generating back end is intended for
         source-to-source transformation applications.  It takes the unlowered
         IL for a translation unit and generates a source file that is similar
         to the original C or C++ input.
   * - | ``MINIMAL_INLINING``
     - | TRUE if minimal inlining of function calls should be done during IL
         lowering.  This option is intended mostly for use with the
         C-generating back end, and is limited to inlining calls to very simple
         functions that are defined inline in the source.

Support for Source Language Dialects
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::

   * - | ``GNU_EXTENSIONS_ALLOWED``
       | ``DEFAULT_GNU_COMPATIBILITY``
       |
     - | Control whether compatibility with GNU's ``gcc`` and ``g++`` compilers
         should be enabled, either by default or only when the ``--gcc`` or
         ``--g++`` command-line options are specified.
   * - | ``MICROSOFT_EXTENSIONS_ALLOWED``
       | ``DEFAULT_MICROSOFT_COMPATIBILITY``
       |
     - | Control whether compatibility with Microsoft's C and C++ compilers
         should be enabled, either by default or only when the ``--microsoft``
         command-line option is specified.
   * - | ``SUN_EXTENSIONS_ALLOWED``
       | ``DEFAULT_SUN_COMPATIBILITY``
       |
     - | Control whether compatibility with Sun CC should be enabled, either by
         default or only when the ``--sun`` command-line option is specified.
   * - | ``DEFAULT_CLANG_COMPATIBILITY``
       |
     - | Controls whether compatibility with clang C and C++ should be
         enabled by default or only when the ``--clang`` command-line
         option is specified. (Clang extensions are mostly identical to GNU
         extensions and are controlled by ``GNU_EXTENSIONS_ALLOWED``.)

Setting an ...\ ``_ALLOWED`` macro to TRUE makes the corresponding
``DEFAULT_``\ ...  macro TRUE by default.  If the front end is configured to
support multiple dialects, the ``DEFAULT_``\ ...  macros must be set explicitly
to avoid selecting more than one dialect as the default.

.. list-table::

   * - | ``CPPCLI_ENABLING_POSSIBLE``
       | ``CPPCX_ENABLING_POSSIBLE``
       |
     - | Control whether a mode compatible with Microsoft's C++/CLI or
         C++/CX extensions should be available through the command-line
         options ``--cppcli``/``--cppcx``.  Setting either of these options
         to TRUE requires that ``MICROSOFT_EXTENSIONS_ALLOWED`` also be
         TRUE.
   * - | ``DEFAULT_CPPCLI_ENABLED``
       | ``DEFAULT_CPPCX_ENABLED``
       |
     - | TRUE if C++/CLI or C++/CX extensions should be enabled by default
         when Microsoft extensions are enabled.

IL Features
^^^^^^^^^^^

.. list-table::

   * - | ``EXTRA_SOURCE_POSITIONS_IN_IL``
       |
     - | TRUE if certain additional IL entries, besides those that already have
         ``a_source_correspondence`` fields, should contain source position
         information.  For example, expression nodes will record the starting
         and ending position of the corresponding expression, as well as the
         position of that expression's top-level operator.  Note that this can
         increase the size of the IL substantially.
   * - | ``EXPR_RANGE_MODIFIERS_IN_IL``
       |
     - | TRUE to extend the tracking of source ranges for expressions by
         including position information for operators that generally do not
         appear in the IL, such as ``*``, ``&``, and ``()``.
   * - | ``RECORD_MACROS_IN_IL``
       | ``RECORD_MACRO_INVOCATIONS``
       | ``RECORD_MACRO_ARGS``
       | ``FULLY_RESOLVED_MACRO_POSITIONS``
       | ``MACRO_INVOCATION_TREE_IN_IL``
       |
     - | Indicate whether extra information should be recorded in the IL
         describing macros and their expansions. Setting
         ``RECORD_MACRO_ARGS`` to TRUE requires that
         ``RECORD_MACRO_INVOCATIONS`` be set to TRUE.  Setting either of
         the latter two configuration macros to TRUE can increase the size
         of the IL substantially.
   * - | ``ALL_TEMPLATE_INFO_IN_IL``
       |
     - | TRUE if prototype template instantiations (i.e., the parsed generic
         form of templates) should be recorded in the main IL tree.

Target Platform Options
^^^^^^^^^^^^^^^^^^^^^^^

Note that although these configuration macros deal with issues related to the
target, they are not considered "target-specific" (because these values cannot
differ between target configurations).

.. list-table::

   * - | ``IA64_ABI``
     - | Controls whether the IA-64 ABI standard is used for code
         generation and object layout.  This is a "modern" C++ object
         layout (unlike the cfront layout), and is a good starting point
         even on architectures other than IA-64 (Itanium).  This ABI is
         used by many versions of g++ (3.2 and later) and clang.  For a
         complete specification, see ``www.codesourcery.com/cxx-abi``.  See
         also other macro names beginning with ``IA64_ABI``, some of which
         enable compatibility with the ARM EABI variant of the
         ``IA64_ABI``.  If ``IA64_ABI`` is set to FALSE (or 0), a
         cfront-like ABI is used instead.
   * - | ``DEFAULT_GNU_ABI_VERSION``
       | ``DEFAULT_EMULATE_GNU_ABI_BUGS``
       |
     - | Control the emulation of the GNU variant of the IA-64 ABI.
   * - | ``GCC_IS_GENERATED_CODE_TARGET``
       | ``GNU_TARGET_VERSION_NUMBER``
       | ``MSVC_IS_GENERATED_CODE_TARGET``
       | ``MSVC_TARGET_VERSION_NUMBER``
       | ``SUN_IS_GENERATED_CODE_TARGET``
       | ``SUN_TARGET_VERSION_NUMBER``
       | ``CLANG_IS_GENERATED_CODE_TARGET``
       | ``CLANG_TARGET_VERSION_NUMBER``
       |
     - | Control whether the code generated by the C- or C++-generating
         back end should target a specific GNU, Microsoft, Sun, or clang
         compiler.  This is information is used to avoid limitations of
         those compilers and to exploit extensions they provide.
   * - | ``CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT``
       |
     - | TRUE if the C++-generating back end should generate code that matches
         the dialect selected for the front end.  (E.g., if the front end is
         set to parse GNU code, the back end can generate GNU ``__attribute``
         constructs, whereas if the front end is set to accept Microsoft
         extensions, the back end might generate ``__declspec`` specifiers.)

Front End Behaviors
^^^^^^^^^^^^^^^^^^^

.. list-table::

   * - | ``MAKE_FRONT_END_CALLABLE``
       |
     - | TRUE if, instead of having its own main program, the front end is to
         be linked as part of some other main program.  When this flag is set,
         the ``EDG_MAIN`` macro provides the name of the main entry point into
         the front end.The front end can then be called repeatedly by the same
         process (each call reinitializes the front end's internal state).
   * - | ``COMPILE_MULTIPLE_SOURCE_FILES``
       |
     - | Indicates that the front end should be capable of compiling a list of
         source files in a single invocation.
   * - | ``IL_SHOULD_BE_WRITTEN_TO_FILE``
       |
     - | TRUE if the intermediate language should be written to a file; FALSE
         if the intermediate language is passed in memory to the back end
         (assuming ``BACK_END_SHOULD_BE_CALLED`` is TRUE).
   * - | ``USING_DRIVER``
     - | Indicates that the front end is being run from a driver program.
         Setting this flag suppresses termination messages (like
         "``Compilation terminated.``") written to the error output file
         because it is expected that the driver will write those.
   * - | ``MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED``
       |
     - | TRUE if multibyte character sequences (e.g., like Unicode UTF-8 or
         those in the Japanese SJIS encoding) are supported in comments,
         string literals, identifiers, and character constants.
   * - | ``USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING``
       |
     - | TRUE if, instead of the routines in the standard C library (e.g.,
         ``mblen``), custom routines in ``host_envir.c`` should be used to deal
         with Japanese SJIS multibyte characters in source code.
   * - | ``LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED``
       |
     - | Indicates the locale to be established by a call to ``setlocale`` to
         get the desired processing of multibyte characters from the C library
         while reading the source code.
   * - | ``UNICODE_SOURCE_SUPPORTED``
       |
     - | TRUE if the character set to be supported is Unicode.  When
         ``MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED`` is TRUE, source files
         encoded as UTF-8 and UTF-16 are accepted, depending on the
         ``--unicode_source_kind`` command-line option,
         ``DEFAULT_UNICODE_SOURCE_KIND`` configuration macro, or Byte Order
         Mark (BOM), and UTF-16 is translated to UTF-8 immediately on
         input. When ``MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED`` is FALSE, the
         source file is assumed to be encoded as ISO/IEC 8859-1 (Latin-1).

Development Aids
^^^^^^^^^^^^^^^^

.. list-table::

   * - | ``DEBUG``
     - | Enables additional code in the front end to simplify debugging.  This
         includes flow/event tracing options, routines that can be called from
         a debugger to examine the IL, and an option to output the settings of
         the configuration macros.
   * - | ``CHECKING``
     - | Enables inexpensive internal consistency checking.  Recommended even
         in production builds.
   * - | ``EXPENSIVE_CHECKING``
     - | Enables more expensive internal consistency checking.  (Not generally
         recommended for production builds.)
   * - | ``WRITE_CPPCLI_PORTABLE_ASSEMBLIES``
       |
     - | When TRUE, this enables an internal command-line option
         "``--set_flag generate_portable_assemblies``" that causes a front
         end built with support for C++/CLI (for Microsoft Windows) to
         write out all assemblies used in the compilation (likely including
         ``mscorlib.dll``) in the current directory in a format that is
         readable on non-Windows platforms (i.e., with a front end not
         built with ``EDG_WIN32`` set to TRUE).  Since the assemblies are
         written in the current directory with their original file name
         (e.g., "``mscorlib.dll``"), care should be taken that no required
         assemblies are in the current directory when the front end is
         invoked in this way.

.. _target-specific-configuration:

Target-Specific Configuration
-----------------------------

A target configuration is defined by giving appropriate values to a set of
configuration macros (dubbed the target-specific configuration macros).
Multiple target configurations can be specified when the front end is built;
the ``--target`` command-line option is used to select a non-default target
configuration when the front end is invoked.  The ``--dump_legacy_as_target``
command-line option can be used to display the set of target-configuration
macros.

Each target-specific configuration macro has a "legacy" version, i.e., one
without any target-specific suffix, as well as a version for each target
configuration that is specified when the front end is built.  Non-legacy
configuration macro names are formed by appending an underscore and the target
configuration name to the legacy configuration macro name.  For example, the
legacy macro ``TARG_LITTLE_ENDIAN`` would have a corresponding
``TARG_LITTLE_ENDIAN_win32`` configuration macro for the ``win32`` target
configuration.

By default, the legacy configuration has no name, but one may be specified by
setting ``LEGACY_TARGET_CONFIGUATION_NAME`` to the desired name.  The default
target configuration can be specified by giving its name as the definition of
the ``DEFAULT_TARGET_CONFIGURATION_NAME`` configuration macro; otherwise, the
legacy target configuration will be used as the default.

Note that specifying a named target configuration (either explicitly or through
defaults) requires that an ``$EDG_BASE/lib_``\ *target*\ ``/`` directory be
present at run time.  Having target-specific ``$EDG_BASE/lib_``\ *target*\
``/`` directories allows each target configuration to have different predefined
macros (e.g., to define ``_M_IX86`` in one and ``_M_X64`` in another).

A legacy configuration can be configured manually (by specifying values for
each target-specific macro), or automatically (by using the ``dettarg`` tool
and defaults that many configuration macros have).  The
``--dump_configuration`` command-line option provides a convenient display of
all configuration macros and their settings.

A non-legacy target configuration is most easily generated as a derivative from
an existing legacy target configuration using the ``--dump_legacy_as_target``
command-line option.  This option takes a target configuration name and
generates a list of the target-specific macro names for the given target
configuration name along with the values of the legacy configuration.  This
file (after verifying that ``TARGET_CONFIGURATION_``\ *number* is unique) can
then be included in ``defines.h`` as the basis for a new target configuration.
Additional target configurations can be generated from different legacy
configurations in the same manner, or by using an existing target configuration
as a template (and changing the target configuration name in each macro, along
with the ``TARGET_CONFIGURATION_``\ *number*).  Note that all target-specific
configurations must share the same non-target-specific configuration, so for
instance, it's not possible to have an IA-64 ABI target configuration and a
Cfront target configuration in the same front end configuration (because
``IA64_ABI`` is not a target-specific configuration macro and must have the
same value across all target configurations).

Note that it is inadvisable to use a target-specific global variable as the
value for a target-specific configuration macro.  For example:

  ``#define TARG_IA64_VTABLE_ENTRY_INT_KIND targ_ptrdiff_t_int_kind``

is subject to race conditions (because ``targ_ptrdiff_t_int_kind`` may or may
not have been initialized before its value is used to set
``targ_ia64_vtable_entry_int_kind``).  The following will work as expected:

  ``#define TARG_IA64_VTABLE_ENTRY_INT_KIND TARG_PTRDIFF_T_INT_KIND``

An important aspect of this architecture is that each legacy configuration
macro has an associated global variable whose value is set at initialization
time to the value of the (legacy or per-target) configuration macro for the
selected target.  Because of this, it is imperative that user modifications to
the front end use the run-time value of global variables rather than the
compile-time values of legacy configuration macros.

Replaceable Code
----------------

Occasionally, some customizations are needed that cannot simply be expressed
through configuration macros.  Those may require instead that various routines
be replaced with custom code.  The following files in ``src/`` are written with
such modifications in mind:

.. list-table::

   * - | ``fixed_pt.c``
     - | Routines implementing operations on fixed-point values (only supported
         in some configurations).
   * - | ``float_pt.c``
     - | Routines implementing operations on floating-point values.
   * - | ``host_envir.c``
     - | Includes routines dealing with operations on files and directories.
         Signal handlers can also be set up there.
   * - | ``sys_predef.c``
     - | Routines to predefine platform-specific entities (macros, types,
         routines, etc.)

Building the Front End
======================

Using the Top-Level Makefile
----------------------------

Assuming a Unix-like environment, a complete demonstration compiler can be
configured and built using a sequence of shell commands like the one shown
below (for a typical Linux platform, but other Unix-like environments are
similar).  The sequence assumes that the current directory is the top-level
directory of a freshly unpacked release.

.. code:: bash

   cp sample_edg_eccp_config/edg_eccp_config.linux edg_eccp_config
   cp src/defines.h.linux src/defines.h
   cd lib
   ../sample_edg_eccp_config/make_g++_incl_paths
   ../misc/make_predef_macro_table
   cd ..
   export EDG_BASE=`pwd` # or setenv EDG_BASE `pwd`
   make

This builds a front end with a C-generating back end, along with a run-time
support library and helper tools such as the prelinker and demangler.  The
entire toolset can be driven with the ``eccp`` script (see
":ref:`the-eccp-script`").

Building the Front End Only
---------------------------

In an environment with a Unix-like ``make`` utility, the front end can be
compiled and linked by running

.. code:: bash

   make edgcpfe

in the ``src/`` directory (after constructing an appropriate ``defines.h`` file
and editing ``src/Makefile`` to select the desired compiler and compiler
options for building the front end).  This can be useful to update the front
end for modifications after building the complete tool set, or when only the
front end program itself is desired.  If ``make`` is not available, the front
end can be built using the following manual steps.

* | Configure the ``defines.h`` file.  For example, under Windows the following
    is a useful starting point:

  .. code:: powershell

     copy src\defines.h.win32 src\defines.h

* | In the ``misc/`` directory, compile and link the ``mk_errinfo`` program
    into a ``mk_errinfo`` executable file.  For example, using a Windows
    command line and the Microsoft C++ compiler:

  .. code:: powershell

     cd misc
     cl /TP /I..\src mk_errinfo.c
     cd ..

  | See :ref:`mk-errinfo` for additional information about ``mk_errinfo`` and
    the format of its input files.
* | In the ``src/`` directory, translate the diagnostic messages using the
    ``mk_errinfo`` program that was built in the previous step.  For example,
    using a Windows command line

  .. code:: powershell

     cd src
     ..\misc\mk_errinfo error_msg.txt error_tag.txt err_codes.h err_data.h

* | Still in the ``src/`` directory, compile and link all the ``.c`` and
    ``.cpp`` files.  For example:

  .. code:: powershell

     cl /TP /EHsc /Feedgcpfe.exe *.c *.cpp /link mscoree.lib oleaut32.lib

Building Support for C++/CLI and C++/CX Modes
---------------------------------------------

C++/CLI is a Microsoft extension to C++ that simplifies writing C++-like
programs for Microsoft's ".NET" environment.  It is formally specified by the
ECMA standard ECMA-372.  The front end can support this extension, provided it
is built on a sufficiently recent Microsoft Windows platform with a compiler
that is sufficiently compatible with Microsoft's "Visual C++" compiler.  This
support is currently limited to producing high-level IL.  The C++-generating
back end can consume this IL (and render the C++/CLI constructs it describes),
but lowering and the C-generating back end are not available in C++/CLI mode.

C++/CX is a Microsoft extension to C++ similar to C++/CLI, but it targets the
"Windows Runtime" (WinRT) platform instead of the ".NET" environment.  Unlike
C++/CLI, there is no formal document specifying this extension.  The front end
can also support C++/CX, with the same caveats as those mentioned for C++/CLI
support (in particular, IL lowering is not available for C++/CX mode).

To include support for C++/CLI and/or C++/CX the configuration macros
``CPPCLI_ENABLING_POSSIBLE`` and/or ``CPPCX_ENABLING_POSSIBLE``
(respectively) must be set to TRUE.  (The macros
``MICROSOFT_EXTENSIONS_ALLOWED`` and ``EDG_WIN32`` must also be TRUE when
building such configurations.) In addition to compiling the C++ files
constituting the front end proper (as explained above), support for C++/CLI
and/or C++/CX requires that the C++ source file ``ms_metadata.cpp`` be
compiled and the resulting object file must be added to the set of files
linked into the final front end executable (or library).  The system
libraries ``mscoree.lib`` and ``oleaut32.lib`` are then also required in
the final executable.

Compiling ``ms_metadata.cpp`` requires Visual C++ 2012 or later (or a C++
compiler and libraries that are sufficiently compatible with that).  The free
"Express" version of Visual C++ is not sufficient because it doesn't include
the ATLMFC library, which ``ms_metadata.cpp`` depends on.

Building and Using the IL Display Program
-----------------------------------------

For educational and debugging purposes, the front end comes with a utility
named ``edgcpdisp`` that reads a file containing the IL produced when the front
end compiles one or more source files and displays it in a human-readable form.
(In order to use this facility, of course, the front end must be configured to
write an IL file, i.e., ``IL_SHOULD_BE_WRITTEN_TO_FILE`` must be set to TRUE in
``defines.h`` or in ``src/Makefile``.)

Because the IL file written by the front end is essentially a binary dump of
the IL data structures, it is necessary for the front end and ``edgcpdisp`` to
be built on the same platform and using the same configuration options.  (Many
of the IL data structures contain fields that are present only in certain
configurations.)

In an environment with a Unix-like ``make`` utility, ``edgcpdisp`` is
automatically built (with a configuration that correctly matches that of
the front end) when ``make`` is run for the default target in either the
top-level or ``src/``\ directory.  It can also be built separately in the
``src/`` directory by specifying ``edgcpdisp`` as the target.

If ``make`` is not available, ``edgcpdisp`` can be built after the front end is
built (as described in the preceding section) in the ``src/``\ directory by
compiling a subset of the source files with ``STANDALONE_IL_DISPLAY`` set to
TRUE.  For example, on a Windows platform using the Microsoft C++ compiler, the
command would be (all on one line):

.. code:: powershell

 cl /TP /Feedgcpdisp.exe -DSTANDALONE_IL_DISPLAY const_ints.c  debug.c      \
    error.c fixed_pt.c float_pt.c host_envir.c  il.c il_display.c il_read.c \
    il_to_str.c il_walk.c  mem_manage.c target.c types.c fe_init.c

Displaying the IL for a source file ``x.cpp`` can be accomplished with the
following commands:

.. code:: bash

   edgcpfe --output x.cil x.cpp
   edgcpdisp x.cil

(In configurations that do IL lowering, ``x.cil`` will contain the lowered IL.
To view the unlowered IL, see the ``--no_il_lowering`` front-end command-line
option described in the next chapter.)

The format of the output is partially controlled by the configuration macro
``ALTERNATIVE_IL_FILE_FORMAT``. When that macro is set to TRUE, each entry
is given a unique numeric identifier, resulting in output like the
following:

.. code:: text

  ...
  file-scope type#6
  source_corresp:
    name:                  file-scope id-name#17: "type_info"
  ...

When ``ALTERNATIVE_IL_FILE_FORMAT`` is FALSE, the entries are
distinguished by a hexadecimal representation of the entry's address:

.. code:: text

  ...
  file-scope type@a0013ee68
  source_corresp:
    name:                  file-scope id-name@a0013f050: "type_info"
  ...

It is not necessary to use the ``edgcpdisp`` to get a human-readable
display of the IL. When it is built with the configuration macro
``NEED_IL_DISPLAY`` set to TRUE, the front end will put out the IL display
on ``stdout`` (using the hexadecimal-address format) when the
``--il_display`` command-line option is specified. (The IL need not be
written to a file in this case.)

Driving the Front End
=====================

For many applications, it is useful to invoke the front end through another
"driver" program.  One such driver is the ``bin/eccp`` script that is part of
every release.  Another is the ``edgcc`` program that can be downloaded
separately from the EDG download site.

.. _the-eccp-script:

The ``eccp`` script
-------------------

The ``bin/`` directory contains a (Bourne) shell script ``eccp`` that
allows the front end to be used much like most Unix-hosted compilers.  It
invokes the front end -- which must be configured with the C-generating back
end -- with various default options, and then compiles any generated C files
through a "native" C compiler (e.g., the GNU ``gcc`` compiler) and a native
linker.  The script accepts Unix-like compiler options (such as ``-c``,
``-o``, ``-O``, ``-g``, ``-I``, ``-L``, and ``-l``) and also passes through
most of the options handled by the front end proper (see :ref:`cli`).

The script can be moved anywhere, but it normally looks for various components
(the front end, the prelinker, etc.) under a directory designated by the
environment variable ``EDG_BASE``.  The ``eccp`` script can be edited to set a
default substitute value for the ``EDG_BASE`` variable.  Normally, the value of
``EDG_BASE`` is the top level directory of a fully-built release tree (although
the ``src/`` and ``lib_src/`` directories are not used by ``eccp``).  ``eccp``
will start by setting various environment variables by executing a script
``$EDG_BASE/edg_eccp_config``.  Sample scripts for various platforms are
provided in the ``sample_edg_eccp_config/`` directory.  Two environment
variables commonly customized in the ``edg_eccp_config`` script are
``EDG_DEFAULT_DEFINES`` (to add options like ``-D__unix__``) and
``EDG_C_TO_OBJ_COMPILER`` (to establish which "native" compiler to use; e.g.,
``gcc``).

``eccp`` may also invoke the prelinker (``util/edg_prelink``) to automatically
instantiate templates (and, with some front end configurations, inline
functions that need an out-of-line copy).  See
:ref:`template-auto-instantiation` for notes on this process.  The script will
also run ``util/edg_munch`` if needed, and filter linker errors through
``util/edg_decode``.

See ":ref:`utility-programs`" for more information on the utility programs
``edg_prelink``, ``edg_munch``, and ``edg_decode`` used by the ``eccp`` script.

The ``edgcc`` Driver for Windows
--------------------------------

A Windows program implementing a subset of ``eccp``\ 's functionality can be
downloaded from the EDG download site.  The file to download is
``nt_util.zip``.  In addition to source code for the driver program ``edgcc``,
it also contains source code for a Windows-hosted prelinker (``pl_nm``) and
munch-like program (``munch_nm``).

The plain text file ``src/msinfo.txt`` contains additional notes for
running the front end in the Windows environment.

Configuring and Building the Run-Time Support Library
=====================================================

Source code for a sample run-time support library is provided in the
``lib_src/`` directory.  This library is meant to be built using the front end
(i.e., a compiler incorporating the EDG front end).  The library is useful for
demonstration and debugging purposes.  However, it is normally not suitable for
production compilers because it is written in portable C++ that cannot take
advantage of more efficient platform-specific mechanisms (e.g., to implement
exception handling).

Similarly to the front end, the library can be configured through macro
definitions placed in ``lib_src/defines.h``.  Typically, however, few or no
macros need to be defined because the ``--building_runtime`` option to the
front end (used to build this library) causes most or all required macros
to be predeclared.

The macros that might need manual configuration are documented in
``lib_src/config.h``.

When using multiple target configurations, a version of the run-time support
library must be built for each target configuration and installed in the
appropriate ``lib_``\ *target*\ ``/`` directory.  See ``lib_src/Makefile`` for
directions to build target-specific versions of the library.
