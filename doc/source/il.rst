.. _intermediate-language:

=====================
Intermediate Language
=====================

The Intermediate Language is the output of the front end.  It is a
representation of the source program that is passed to the back end, and
from which the back end can perform analyses, make transformations, and/or
generate object code.

The intermediate language is a "high-level" intermediate language, that is, a
representation that is close in spirit to the original source language, rather
than a "low-level" intermediate language that is close to any typical computer
architecture.  That fits the EDG philosophy of the function of a front end,
i.e., that a front end should determine the meaning of a source program and
produce an intermediate language representation of the source program that:

* | accurately represents the meaning of the program;
* | does not destroy information;
* | does not add constraints that are not present in the source program; and
* | makes explicit any implicit or overloaded operations in the source program,
    so that the back end does not have to have a deep understanding of the
    source language and does not have to search or interpret the intermediate
    language.  It follows from this that the front end does not do
    optimization, does not rewrite source constructs in "simpler" forms, and
    does not use a machine-dependent representation.  It also follows that
    ambiguity and overloading resolution are completely handled by the front
    end and a back end need do nothing to deal with them.  The intermediate
    language is a tree-structured in-memory data structure.  Both declarative
    and executable constructs are represented in tree form, and in fact all
    declarative and executable information at all levels is tied together into
    one tree.  Having a pointer to the root of this tree gives one all the
    information necessary to generate object code for the source program.  (In
    other words, the intermediate language data structure is the only
    information that needs to be passed from the front end to the back end.
    There is no additional hidden information.) The source code definitions of
    the intermediate language tables are in the file ``il_def.h``.  However,
    some introductory information is necessary before one dives into that file.

Memory Management
=================

Memory management may seem like a strange place to begin an overview of the
intermediate language.  However, an understanding of the memory management
structure is necessary to get a feeling for the overall shape of the
intermediate language tree.

Memory Regions
--------------

It was decided early in the design of the front end that the intermediate
language would be an in-memory data structure.  That is, it exists in
directly-addressable memory and can be examined without recourse to special
access routines.  This is beneficial from the point of view of compilation
speed and ease of programming.  However, it could be problematic from the point
of view of memory use, especially when processing extremely large source files.
To deal with that problem, it was decided that the intermediate language should
be set up so that the parts of the tree associated with different top-level
functions are placed in separate blocks of memory that need not all be in
memory at the same time.  A top-level function is any function that is not a
lambda defined in a function scope, or the instantiation of a generic lambda
defined in a function scope.  Functions that are not top-level are placed in
the memory region of the enclosing function.  During the processing of a single
function, the intermediate language for that function (and for other functions
in the same memory region) and for the global declarations visible to that
function must be in memory.  The intermediate language for all other functions,
however, is not needed at that time, and can be written out to a file or
otherwise removed from the active address space.  As a result, a large source
program looks more like a sequence of reasonably-sized functions and uses
memory accordingly.

These blocks of memory are called "memory regions".  There is a memory region
for the file scope intermediate language and one for the intermediate language
of each top-level function of the source program.

The memory region scheme is a two-level scheme: there is a file scope memory
region which is always in memory, and a set of function scope memory regions of
which only one is guaranteed to be in memory at a given time.  C++ lambdas
defined in function scopes are handled by placing them in the same memory
region as the enclosing function.  Member functions of local classes in C++,
while somewhat similar to lambdas, are treated (with some careful allocation of
entities) as top-level functions and have their own memory regions.

Memory Regions and Pointers
---------------------------

The memory management structure has an effect on pointers in the intermediate
language.  Clearly, a pointer to an entry in a function scope memory region is
valid only if the associated memory region is actually in memory.  Rather than
requiring dynamic checking of pointer legality, we decided to make a rule about
how pointers may be used.  Specifically, entries within a function scope memory
region may only be pointed to from within that memory region.  Entries in the
file scope memory region may, of course, be pointed to from anywhere, since
they are always in memory.

This rule has some consequences.  The most important one is that anything that
must be referenced from more than one memory region must be allocated in the
file scope memory region.  Specifically, since functions may be called from
many other functions, they must have some representation in the file scope
memory region.  Of course, since the whole point of the function scope memory
regions is to avoid having everything in memory at one time, it wouldn't make
sense to put the entire routine into the file scope memory region.  What's done
instead is to put the description of the interface to the routine (i.e., what's
needed to call it) in the file scope memory region, and the description of the
implementation of the routine (i.e., what's needed to execute it) in the
function scope memory region.  Similarly, local static variables of functions
must be allocated in the file scope memory region because they are potentially
referenced from member functions of classes local to the function.

A second consequence is that anything referenced from the file scope memory
region must also be allocated in the file scope memory region.  [#f1]_ This
second consequence becomes significant when combined with another intermediate
language decision, that of placing all entities with external visibility at the
file scope.  [#f2]_ Placing the declaration of an external entity at the file
scope means placing all of the entities it references in the file scope memory
region, and that means placing all the entities they reference in the file
scope memory region.  The practical consequence of all that is that all types
must be allocated in the file scope memory region, since it is not possible to
know at the time they are allocated whether or not they will be used as part of
the type of some external entity.  In C++, that means in particular that all
classes are allocated in the file scope memory region (except for the bodies of
member functions).  Likewise, all namespaces are allocated in the file scope
memory region except for the bodies of namespace member functions.

It is important to bear in mind that the *file scope memory region* is not the
same thing as the *file scope*.  They are similar but not identical.  The *file
scope* is a language concept: it is the outermost scope in a C or C++ program.
The *file scope memory region* is a memory management concept referring to a
block of memory whose contents must always be accessible.  All entities in the
file scope are allocated in the file scope memory region, but some entities
allocated in the file scope memory region are function-scope entities, some are
function-scope entities moved to the file scope because they are externally
visible, and some are class or namespace members allocated in the file scope
memory region because all classes and namespaces are allocated there.

Intermediate Language Files
===========================

The intermediate language and memory management scheme are set up to allow but
not require the use of files to save the intermediate language.  The front
end's connection to a back end can be configured in a number of ways:

* | The entire intermediate language tree can be passed in memory to the back
    end.  This is fastest but uses the most memory.
* | The intermediate language can be written to a file on a
    per-top-level-function basis and read back in on a per-top-level-function
    basis in the back end.  (Only one file is used, but the intermediate
    language for each function is written separately and the memory involved is
    reclaimed immediately.) This is slowest but uses the least memory.
* | The intermediate language can be processed on a per--top-level-function
    basis in some other way, then thrown away.  For example, the intermediate
    language can be translated to another intermediate language used by the
    back end, and the original intermediate language can be discarded.

C versus C++ Intermediate language
==================================

The intermediate language generated for C++ is a superset of the intermediate
language generated for C.

For the sake of customers that have an existing C back end, EDG provides a
optional and separate "IL lowering" pass that translates the C++ intermediate
language into the C intermediate language.  Note that although the process is
called "lowering," the resulting C intermediate language is still "high-level";
it's just not as high-level as it was.  Also note that the IL lowering process
does some implementation of C++ features (e.g., virtual function tables, name
mangling, pointers-to-members, constructor and destructor wrapper code) which
are just abstractions in the unlowered IL.  Clearly, a back end has the most
latitude in generating code for C++ if it starts with the unlowered C++ IL, but
it also has quite a bit more work to do.  On the other hand, an extra pass over
the intermediate language takes time, so a configuration that uses the IL
lowering pass will not be as fast as one that does not use it.

The IL lowering pass is useful because

#. it allows customers to get a C++ compiler going in minimal time;
#. if the features implemented by IL lowering require some minor changes,
   the changes can be made in IL lowering, which is relatively small, rather
   than in the front end as a whole, which is quite large; and
#. if a customer wants to attach a back end to the unlowered IL (either
   initially or for an improved release of a compiler), the IL lowering code
   serves as a guide to the tasks that must be done.

The description of the intermediate language in this chapter is for the
unlowered IL.  All C++-specific features are rewritten or removed by the IL
lowering, which means that some of the description in this chapter is of no
interest to a back end that uses the lowered IL.  If there is some question
about whether or not a feature would remain in the lowered IL, the description
will indicate what effect IL lowering has.

There is no way at present to compile the C++ front end so that the IL that it
generates includes only the C IL definitions.  Even though IL lowering will
eliminate uses of non-C features, the C++-specific fields and enumeration
values are still present in the source code.  Back ends should ignore those
fields (they are not necessarily cleared) and should expect to get IL that is
valid C when one considers (only) the fields that C uses.

One catch: Dynamic initialization of entities in the middle of blocks is left
in its original form.  This is not "incorrect" C IL, but is something that does
not come up in C(89) programs.  It was judged that back ends would be capable
of dealing with the ``stmk_init`` statements even if they appear after other
executable statements in a block, and the alternative was to make lowering of
initialization of aggregates to constants quite messy.

Similarly, the initialization statement attached to an ``stmk_for`` is always a
simple ``stmk_expr`` in C, but in C IL generated by IL lowering, it can be an
arbitrary statement.

C99 and the Intermediate Language
=================================

The ANSI/ISO C standard was updated in 1999, and the resulting version of C
is referred to as C99.  The C/C++ front end supports C99 (as well as later
versions).  In some cases, the IL has been extended to support newly-added
features of C99.

The IL extensions for C99 and later revisions are enabled by the macro
``C99_IL_EXTENSIONS_SUPPORTED``.  If that macro is set to FALSE, C99 and
later modes can still be enabled, but all features that require the IL
extensions are disabled.  Note, in particular, that if your interest is
only in supporting C++, or only C89, turning off these features will make
it slightly easier to write a back end, because you will have to implement
fewer IL constructs.

There is a lowering phase for C99, which rewrites many of the extensions into
standard C89 IL.  This lowering phase is enabled by the macro
``DO_IL_LOWERING``.

See :ref:`il-c99-features` for a detailed list of C99 IL constructs.  That list
summarizes the lowering done for each construct; there is more detail in the IL
Lowering chapter of this document.

C++/CLI and the Intermediate Language
=====================================

The front end supports the Microsoft C++/CLI language, a set of extensions to
C++ to allow programmers to interface to Microsoft's .NET, which is a
heap-based garbage-collected environment in the style of Java or C#.  There is
an ECMA standard for the C++/CLI language (ECMA 372), but the real standard is
the MSVC compiler, as it diverges from the ECMA document in significant ways.

The C++/CLI extensions involve new types, new expression operators, new member
function kinds, new declarations, and new statements.  Those are covered in
detail in the appropriate sections of this document.  The data structures and
code for those features are included when ``MICROSOFT_EXTENSIONS_ALLOWED`` is
TRUE; there is no separate macro for C++/CLI to control their inclusion.  The
features are enabled only when ``cppcli_enabled`` is TRUE, and there is a macro
``CPPCLI_ENABLING_POSSIBLE`` that when FALSE (the default) prevents the C++/CLI
features from being enabled.  So if you do not want to support C++/CLI, leave
``CPPCLI_ENABLING_POSSIBLE`` set to FALSE and you will never see any of its
features in the IL.

Note that C++/CLI is supported only when the front end is hosted on Windows.
Also note that C++/CLI is Microsoft's second version of a C++ interface for
.NET; there was a previous set of extensions called "Managed C++".  The front
end does not support those features (and Microsoft is moving away from
supporting them as well).

Memory Region Processing Order
==============================

When a file is compiled, the processing order of memory regions will look like:

* | Start file scope memory region (1).

  * | Start function scope memory region (2).
  * | End function scope memory region (2).
  * | Start function scope memory region (3).
  * | End function scope memory region (3).
  * | Etc.
* | End file scope memory region (1).

When IL lowering is done, it is done as each memory region is ended.  When IL
files are written, they are written as each memory region is ended (after IL
lowering, if any).

When the front end instantiates functions or generates routines (like
constructors), there can be brief periods when multiple function scope memory
regions are active.  The function scope memory region for the instantiated or
generated routine is started and ended while the primary function scope memory
region is still active.  If this process kicks off instantiation or generation
of other routines, there can be many active function scope memory regions.

General Information
===================

Each major type of entity in the C/C++ language has a corresponding
intermediate language entry.  For example: ``a_constant``, ``a_type``,
``a_variable``, ``a_routine``, ``a_label``, ``a_statement``, and
``an_expr_node``.  Each entry kind has a fixed size except for the entries
representing strings (used for literal strings and names), which have a
variable length.  The IL entries are not self-labeling; one must know the kind
of thing pointed to to make sense of a pointer.

The intermediate language is high-level, meaning that the entities in it
correspond fairly closely to C and C++ language constructs.  For example, all
of the C and C++ expression operators are present in their original forms.
There is no remapping of complex operators (like "``?``") to lower-level
operations.  Statements are kept in their original structured form, and not
flattened to ``if``\ s and ``goto``\ s.

Since the structure of the intermediate language corresponds so closely to the
original C or C++ form, it implies no ordering of evaluation that was not in
the original program.  Timing of side effects for post-increment operations,
for example, which must be completed by the next sequence point, is not
specified explicitly.  It is up to the back end to adhere to the C and C++
language rules when generating object code for such constructs.

By convention, there can be multiple pointers to any given declarative entity
(for example, many places can point to the constant entry for ``1``), but only
a single pointer to any executable entity (each use of ``1`` in an expression
requires a distinct expression node pointing to the common constant entry for
``1``).

The intermediate language tables are designed to keep memory requirements
reasonably low.  There are several cases where ``enum`` values are stored as
``a_byte``\ s (which means as ``char``\ s of some kind), in order to ensure
that only one byte is used for those rather than whatever the host C compiler
might choose for an ``enum``.  An example of such a type is
``a_constant_repr_kind``.  This has the unfortunate side effect that explicit
casts are often required when dealing with values of these types.  There are
also several cases where, in a union that has one case that is larger than all
the others, the extra information for the unusual case is broken out as a
separate supplement that is pointed to from the union (e.g.,
``a_routine_type_supplement``).

As mentioned, all of the intermediate language is tree-structured.  The
expression ``i+1`` is a tree with a ``+`` expression node pointing to two
expression nodes representing ``i`` and ``1``; the statement

.. code:: c++

   if (i) i=1; else i=2;

is a tree with an ``if`` statement node pointing to two statement nodes for
``i=1`` and ``i=2``; and the declaration

.. code:: c++

   int *m[5];

is a variable node for ``m`` pointing to a tree defining the type "array of
pointer to ``int``".  All the statements of a function are bundled into one
tree which is pointed to by the function definition, and all the type,
variable, and function definitions for a given scope are on a list pointed to
by the header for the scope.  At the top of the entire tree for a compilation
there is a header that points to the entry for the outermost scope.  Beginning
there, one can get to all of the functions and therefore all of the functions'
scopes; one can also get to all of the classes and therefore all of the
classes' scopes.  In other words, beginning at the IL header one can reach the
entire IL tree for the compilation.

The IL Header
=============

The top of the intermediate language tree is ``il_header``, which contains
several important pointers:

* | A pointer to the scope entry defining the primary scope of the compilation.
    This is the root of the IL tree proper.
* | A pointer to the top of a tree of entries describing the source files from
    which the source program came.
* | A pointer to the ``scope_region_entry`` array, which gives pointers to the
    scope entries that are the top of the IL tree in each of the function scope
    memory regions.
* | A pointer to the ``function_def_table`` array, which gives the pointers to
    the scope entries and memory region numbers for a given function
    definition.

Source File Information
=======================

The source file information structure describes the primary source input file
and the include files of the compilation, and the relationship between them.
It is a structure distinct from the IL tree proper because the file structure
of a program need not have any relationship to its syntactic structure.  An
include file, for example, could include half of a routine, or the end of one
routine and the start of another.  (This is of course not typical, but it's
possible.) Within the IL tree proper, a source line's position is given by a
*sequence number*, which is the ordinal position of the source line in the
sequence of lines read by the front end, starting at the beginning of the
primary source file and switching into include files when directed to by
``#include`` directives.  The sequence number is compact and unambiguous for
internal use, and can be translated easily to a file name and line number for
external use by referring to the source line information structure.  The
routine ``conv_seq_to_file_and_line`` in ``il.c`` does that translation.

Source Correspondence Information
=================================

Constructs that declare constants, types, variables, routines, fields, labels,
namespaces, templates, template parameters, and macros have information on the
associated source name and the source location of the declaration.  This
information is given by the ``source_corresp`` field, which is a ``struct`` of
type ``a_source_correspondence``.  A source location is specified by an entry
of type ``a_source_position``, which consists of a sequence number and a column
number.  The column number is the 1-origined column number in the original
source line (*not* the logical source line that results after processing
trigraphs and line splices).  Tab characters count as one column, so the
"column number" may not match the apparent display position on a terminal or
printer.  If multibyte characters appear, including characters that come from
files in formats such as UTF-16, they count as a single column.  The node for
statements has a source location as well, but (to save space) it contains only
a sequence number and no column number (see, however, the configuration flag
``FULL_SOURCE_POS_IN_IL_STATEMENT``).

Aside from a pointer to the name string and the source position, the source
correspondence information contains an ``assoc_info`` pointer.  This is
intended to be used by the front end and other components to store a pointer to
information maintained locally about the entity.  However, the information does
not survive into the next pass.  In the front end, the pointer points back to
an associated symbol entry.  That pointer is no longer valid once the front end
is finished (since the space for symbol entries may have been released), but
the pointer can be set in the back end as desired.

Compiler-generated entities have no source correspondence information
(since they do not appear in the program source).  Their source
correspondence name pointer is NULL.

The source correspondence information also contains the ``name_linkage`` field,
which indicates the kind of linkage the entity has (e.g., is it externally
visible? Should C++ name mangling be done on it?), an access field (which
indicates the access for class members in C++), a ``parent_scope`` pointer to
the containing scope, and a ``referenced`` field.

When ``IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS`` is TRUE, the name string
in the source correspondence entry can contain multibyte character
sequences (e.g., UTF-8 when ``UNICODE_SOURCE_SUPPORTED`` is TRUE).  When it
is FALSE, multibyte characters will be represented as ``\m``\ *XXXX* or
``\M``\ *XXXXXXXX* escape sequences.  When a universal-character-name
(e.g., ``\u00d6``) appears in a name, the name string will contain the``
\u``\ *XXXX* or ``\U``\ *XXXXXXXX* escape sequence, except that when
``UNICODE_SOURCE_SUPPORTED`` and
``IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS`` are both TRUE the UCN value
will be represented directly.  IL lowering can do some limited rewriting of
such escape sequences; see ``REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING``.  Note
that these flags do not control whether or not multibyte characters are
allowed in identifiers, only how they are represented; see
``MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED``.

``referenced`` and ``needed`` Flags
-----------------------------------

The ``referenced`` flag in a source correspondence entry indicates whether an
entity is referenced.  All entities that have no source correspondence have
``referenced`` set to TRUE.  For named entities, ``referenced`` is TRUE if the
entity is referenced in the intermediate language.  The reference can be
implicit: in a case like

.. code:: c++

   struct s {int a;} v;

the ``struct`` type is referenced, but not through the name ``s``.

The referenced flag is set correctly but simplistically.  More precise
information is available in the ``needed`` flag (and, in classes and routines,
the ``definition_needed`` flag).  These are optional and are selected by the
configuration switch ``MAINTAIN_NEEDED_FLAGS``.  When present, they indicate
whether or not an entity is "really" needed, and whether the definition of a
class is really needed, or merely its declaration.  This is determined by
marking as needed all externally-defined functions and variables, and all the
things they reference, and all the things those things reference, etc.

Depending on the setting of ``DEFAULT_REMOVE_UNNEEDED_ENTITIES``, possibly as
overridden by a command-line option, unneeded entities and unneeded class
definitions will be removed from the IL tree.  This process, however, does not
remove all unneeded entities, because some end up being required for IL
consistency even though they are not "needed." One example: if a class and its
definition are needed, all the members of the class must be kept in the IL even
though some of the members may not be "needed" (in the sense of being
referenced directly or indirectly from an externally defined routine or
variable).  The IL after removal of unneeded entities will be self-consistent,
in that a back end that does not use the needed flags will find nothing
surprising.  If a back end uses the ``needed`` flag, it should also pay
attention to the class ``definition_needed`` flag or it may find some apparent
inconsistencies in the IL tree.

The setting of the "needed" flags and the removal of unneeded IL entries work
both with and without IL lowering.  When IL lowering is done, the setting is
done after lowering, so that the flags reflect the lowered code.

When ``ONE_INSTANTIATION_PER_OBJECT`` is TRUE, a more complicated set of
"needed" flags is maintained.  In that mode, each instantiation is put out as a
separate object file.  There is still only one IL tree, but it is marked so
that a back end can sweep through it several times and put out a different
"slice" each time, each slice containing one instantiated entity (function,
variable, or static data member) plus exactly the set of other entities needed
by that entity.  This is done via a set of per-instantiation "needed" flags,
represented as a bit vector implemented as a linked list of entries of type
``a_per_instantiation_needed_flags_entry`` attached to the
``per_instantiation_needed_flags`` field of the source correspondence.  Each
slice is assigned one bit in that bit vector, and if the bit is 1 the
associated entity is needed in the slice.

See the :ref:`needed-flags` section.

Source positions
----------------

Source position information is recorded in the IL in various ways.  The
``decl_position`` field in the source correspondence indicates the source
position of the primary declaration of an entity.  When an entity has more than
one declaration, ``decl_position`` corresponds to the defining declaration or,
if there is no defining declaration, the first declaration.  When
``GENERATE_SOURCE_SEQUENCE_LISTS`` is TRUE, source positions on declarations
other than the primary declaration are recorded in the source-sequence list --
see ``decl_position`` in ``a_src_seq_secondary_decl``.  As noted previously,
position information is also recorded in ``a_statement``; in addition, source
position information is recorded in other IL entries that have no source
correspondence field, e.g., ``a_base_class``, ``a_using_decl``,
``an_instantiation_directive``, ``a_static_assertion``, and ``a_pragma``.

In applications for which additional source position information is needed,
``EXTRA_SOURCE_POSITIONS_IN_IL`` can be configured to TRUE.  Note, however,
that this information can take a lot of extra space, so the feature is disabled
by default.

When ``EXTRA_SOURCE_POSITIONS_IN_IL`` is TRUE, source range information
(beginning and ending source positions of various syntactic constructs) is
supplied.  Each source correspondence has a pointer to
``a_decl_position_supplement``, which includes the source range of the complete
identifier (including namespace and class qualifiers, if present), as well as
the source range of the declaration-specifiers and the declarator.  For
instance, the source position information for this declaration of ``foo`` is
indicated:

.. code:: text

   inline void A::foo(A *pa) { /* ... */ }
                  ^                    decl_position
               ^^^^^^                  identifier_range
   ^^^^^^^^^^^                         specifiers_range
               ^^^^^^^^^^^^^           declarator_range

Moreover, though the param-type entry for parameter ``pa`` has no source
correspondence entry, it does have a pointer to ``a_decl_position_supplement``,
which records its source range information:

.. code:: text

   inline void A::foo(A *pa) { /* ... */ }
                         ^^            identifier_range
                      ^                specifiers_range
                        ^^^            declarator_range

Source range information is supplied for ``a_base_class`` and
``a_constructor_init``, as well as for the initializer construct of explicitly
initialized variables.  Moreover, when ``GENERATE_SOURCE_SEQUENCE_LISTS`` is
TRUE, pointers to ``a_decl_position_supplement`` also appear in
``a_src_seq_secondary_decl`` entries; this provides source range information on
declarations that are not the primary declaration of an entity (e.g.,
``friend`` declarations).

Source range information is also provided for expression nodes when
``EXTRA_SOURCE_POSITIONS_IN_IL`` is TRUE.  The source positions of the start
and end of the expression, along with the source position of the operator (if
present) are recorded.

Name Linkage
------------

The ``name_linkage`` field of the source correspondence structure indicates the
kind of linkage the name of an entity (routine, variable, class or enumeration
type, or class member) has:

* | External linkage, used in C mode and for ``extern "C"`` entities in C++
    mode, indicates that the name is visible outside its compilation unit.
* | C++ external linkage, for normal entities in C++ mode, indicates that the
    name is visible outside its compilation unit.  The name may be subjected to
    some kind of name mangling.
* | Internal linkage indicates that the entity name is visible throughout its
    compilation unit (e.g., a file-scope static variable).
* | No linkage indicates that the entity name has no connection to other
    entities with the same name (e.g., a local variable).

From a C perspective, the name linkage information seems to be redundant, in
that it would seem that one could decide the name linkage on the basis of a
variable or routine's storage class.  In C++, however, class names (i.e.,
types) have linkage, and in some compatibility modes if a class is used in the
type of an external entity, it acquires external linkage, and its static data
members and member functions also get external linkage.  It gets to be very
complicated to try to encode the storage class (including whether or not it can
be changed on subsequent declarations) and the name linkage in just the storage
class field.

In addition, the ``routine_name_linkage`` field in the routine type supplement
is significant.  The linkage specification in a routine type declaration can
correspond to the routine's calling convention -- for example, when ``extern
"C++"`` and ``extern "C"`` imply distinct calling conventions or an
implementation-defined linkage specification has its own calling convention.
Among other things, this means that in C++ otherwise identical functions are
distinguished for purposes of overloading based on the linkages associated with
their parameter types:

.. code:: c++

   typedef void (*PF)();             // Pointer to an extern "C++" function
   extern "C" typedef void (*PFC)()  // Pointer to an extern "C" function
   void f(PF);
   void f(PFC);                      // Overloads f

Note, moreover, that the linkage specification of a function's *name* need not
be identical to the linkage specification of its *type*.  The former might
affect how its external name is generated and the latter might affect its
calling convention.

Access Control
==============

Access control information appears in several places:

* | The ``access`` field of ``a_source_correspondence``, which appears in most
    declarative entities.
* | ``using``-declarations and access declarations in classes, which may adjust
    the accessibility of inherited members.
* | Base class entries, which indicate the kind of derivation from the base
    class to the derived class.
* | Lists of the friend classes and friend functions of a class.  These are
    also available in reversed form as befriending lists for classes and
    functions, which indicate the classes which have named the befriended class
    or function as a friend.

This information is useful to the back end only for generating symbolic
debugging information.  The necessary access control checks will have been done
by the front end, and access control does not have an effect on code
generation.

Only class members are subject to access control.  All other entities (and all
entities in C) will have access of ``as_public``.

The Scope Entry
===============

Each scope in the source program is represented by a scope entry in the IL
(type ``a_scope``).  There are scope entries for:

* | the file scope;
* | each function definition scope;
* | each block within a function;
* | each "prototype" scope (for function prototypes in function declarations);
* | each class, struct, and union type (when the type has a body);
* | each namespace (in C++); and
* | each condition (tested initialized variable in C++, e.g., in an "``if``"
    statement).

The scope entry for the file scope is pointed to from ``il_header``; it
contains lists of the file-scope entities in the compilation.  In particular,
it points to a list of the functions in the compilation.  Each function is
represented by an ``a_routine`` entry.  A function without a body (i.e., an
``extern`` function) has just a routine entry.  For a function with a body, the
field ``assoc_scope`` in the routine entry gives the region number for the
memory region containing the function, which allows one to get a pointer to the
scope entry for the function scope.  That scope entry points to the statements
for the function, and to lists of its parameter definitions and of local
constants, types, variables, labels, and block scopes.

One can see that the scope entry is the most important entry in terms of the
overall structure of the IL tree, in that it groups entities together into a
scope and also points down to the subscopes.  Conversely, many IL entries point
back to their containing scope via the ``parent_scope`` field of their source
correspondence entry.  (The scope entries themselves also contain a ``parent``
pointer to their enclosing scope; this parent pointer is null for the file
scope.)

A scope entry points to lists of the entities that are defined in the scope:
constants, types, variables, routines, sub-scopes, etc.  The lists may be used
or not depending on the type of scope:

* | File scope (allocated in the file scope memory region):

  * | types: used for tags (``class``, ``struct``, ``union``, and ``enum``) and
      ``typedef``\ s.  Top-level class definitions appear here.
  * | variables: used for static and external variables at the file scope.
  * | routines: used for functions both with and without definitions
      (bodies).
  * | dynamic initializations: used for variables that have dynamic
      initializations (eliminated by IL lowering).
* | Function or block scope (allocated in a function scope memory region):

  * | types: used for tags (``class``, ``struct``, ``union``, and ``enum``) and
      ``typedef``\ s that are local to the function or block.  This pointer is
      unusual in that it will point to a list of entries allocated in the file
      scope memory region.  Local classes appear here.
  * | variables: used for local static variables (one list; this pointer is
      unusual in that it will point to a list of entries allocated in the file
      scope memory region) and automatic/register variables (another list).
  * | scopes: for subscopes (blocks that have declarations).  Note that every
      ``{ }`` has an associated block statement, but a block statement only has
      an associated scope if it contains local declarations.
* | Function prototype scope (allocated in the file scope memory region):

  * | types: used for tags (``class``, ``struct``, ``union``, and ``enum``) and
      ``typedef``\ s that are local to the prototype scope.

  | Used only in C, not C++.  Function prototype scopes are used to contain
    types defined in prototyped parameter lists.  When the function type is
    part of a function declaration (i.e., there is no body), there's nowhere
    else to put the types.  When the function type is part of a definition, the
    prototype scope is effectively part of the function scope, but it must be
    kept separate so that it can be available in the file scope memory region
    as part of the information on the interface to the function.
* | Class scope (allocated in the file scope memory region):

  * | constants: used for class constants (an extension).
  * | types: used for tags (``class``, ``struct``, ``union``, and ``enum``) and
      ``typedef``\ s.  Nested types appear here.
  * | variables: used for static data members of the class.
  * | routines: used for member functions of the class.  This includes all
      member functions: static and non-static, virtual and non-virtual, inline
      and non-inline, constructors, destructors, operator functions, and
      conversion functions.  Friend functions are not listed here (see the
      friend and befriending lists under the class type and routine entries).
* | Namespace scope (allocated in the file scope memory region):

  * | types: used for tags (``class``, ``struct``, ``union``, and ``enum``) and
      ``typedef``\ s.  Type members of the namespace appear here.
  * | variables: used for variable members of the namespace.
  * | routines: used for function members of the namespace.
  * | namespaces: used for nested namespaces.

The entries appear on the lists in the order of declaration in the source
program, with the exception that tags and routines appear at the point where
the full definition occurs, not where the first forward reference appears.
Unreferenced entities *do* appear on the lists.

Not all intermediate language entities appear directly on the lists.  An entity
appears on a list if:

* | it is named in the source program; or
* | it is a tag (``class``, ``struct``, ``union``, or ``enum``); or
* | it is a front-end-generated temporary variable or parameter.

Note that enumerated constants appear as part of the definition of the ``enum``
tag and not ever on a constants list.

The type lists will also contain "placeholder typerefs," which are
``tk_typeref`` types with no name and no type qualifiers which provide
information about corresponding positions in different type lists.  These are
useful to IL lowering in promoting types out of classes and namespaces.  See
the section on placeholder typerefs in the IL lowering chapter of this
document.

For function scopes, the scope entry contains additional information:

* | A pointer to the routine entry (in the file scope memory region).
* | A list of parameter variables, and (for a nonstatic member function) a
    "``this``" parameter variable.
* | For constructors and destructors, a list of ``a_constructor_init`` entries
    indicating initialization or destruction to be done.
* | Labels within the function.
* | A list of entries for computing the bounds of variable length array types.

.. _il-types:

Types
=====

``a_type`` describes a type, which can be ``void``, an integral or enumeration
type, a real, complex, or imaginary floating-point type, a fixed-point type, a
pointer, a reference, ``std::nullptr_t`` (the type of the ``nullptr`` keyword
in C++), an array, a GNU vector type, a ``class`` or ``struct``, a ``union``, a
function type, a pointer-to-member type, a ``typeref``, or a fully generic
template-dependent type.  Some of these type kinds -- including fixed-point and
vector types -- are optional (i.e., available only in specific configurations).

``void``, ``std::nullptr_t``, the fixed-point types, and the floating types are
self-contained.  Integer types and enumeration types point to a supplement
entry of type ``an_integer_type_supplement`` (conveniently accessed with the
macro ``integer_type_supp``).  Enumeration types are basically integer types,
with an additional pointer to either a list of ``a_constant`` entries that
define the members of the enumeration (for unscoped enumeration types), or a
scope entry (for scoped enumeration types; the scope entry then points to the
list of enumerator constants).  The entries representing the enumerator
constants may, e.g.,  be of interest to a back end for generating symbol
debugging information.

The integral, fixed-point, and floating types are provided in a fixed set of
sizes, e.g., ``float``, ``double``, and ``long double`` for floating.  The
sizes of the types can be configured easily, but they remain essentially
symbolic.  That is, ``int`` remains ``int`` and not "integral type of size 4".

A special type kind is used to represent "template parameters" in a broad
sense.  This includes normal template parameters, but also otherwise unknown
template-dependent types like ``typename T::X`` where ``T`` is a template
parameter.  This general concept "template parameter type" is also used to
represent the ``auto`` type specifier in modes that support it.  The general
type kind for template-dependent types is ``tk_template_param``.

The other kinds of types are *derived* types -- they point to one or more other
types, and are used to build up type trees.  For example, the type entry for
``int`` can be pointed to by an array type entry to build an array of ``int``.
A pointer type entry could be put on top of that to make a pointer to array of
``int``.

Derived types are built up from their constituent types in the obvious way:

* | Array types point to the element type and indicate the number of elements.
* | Function types point to the return type.  Parameters of prototyped cases
    (and of the types of defined unprototyped functions) are represented by a
    list of ``a_param_type`` entries (pointed to indirectly by the type entry
    via an entry of type ``a_routine_type_supplement``).
* | Pointer and reference types point to the underlying type.  Pointers and
    references are represented as variants of a common type kind.
* | Pointer-to-member types point to the class type and the underlying type.
* | ``class``, ``struct``, and ``union`` types point to a list of ``a_field``
    entries that define the members.  There is a class type supplement that
    provides a great deal of additional information, especially in C++.  (See
    below.)
* | typedefs, type qualifiers (e.g., const and volatile), type operators such
    as decltype, and various other types are represented by an entry called a
    typeref, described in more detail in a subsection below.

Simple types are shared.  For example, there would only be one type entry for
the type ``int``, and it would be pointed to in all cases where it is needed.
There is also sharing of pointer types, reference types, pointer-to-member
types, and qualified types: the based types list pointer in a type entry points
to a list of types based on the type (e.g., pointer-to the type); when a based
type is created, it is placed on the list and can thereafter be found and
reused.

Types include information indicating their size and alignment (but beware of
``typeref``\ s, which normally have no size or alignment of their own [#f3]_,
and function types, which have no size or alignment).  The sizes and alignments
are set by routines in ``types.c`` and ``class_decl.c`` and are taken on faith
in the rest of the front end.  Incomplete types have size zero and have the
flag ``incomplete`` set to TRUE.  Other types (including classes that contain
no nonstatic data members) normally have sizes greater than zero (some C and
C++ dialects treat some completely defined types as having size 0).

When support for ``near`` and ``far`` is enabled (e.g., in 16-bit Microsoft
mode), the ``near`` and ``far`` memory attributes are encoded as type
qualifiers.  A qualifier appears only if it is not the default.  For example,
if the source specifies ``far`` and the default in that context is ``far``, no
qualifier is added.  Back ends that need to know about memory attributes can
check the size of individual pointers (i.e., does the pointer have the size of
a ``far`` pointer) or use the function ``is_far_type`` (e.g., to determine the
calling method for function types).  The memory attributes appear only when
``il_header.near_and_far_enabled`` is TRUE; see also ``NEAR_AND_FAR_ALLOWED``.

When support for named address spaces is enabled (a feature of Embedded C),
named address spaces are also encoded as type qualifiers.  Unlike other
qualifiers, named address spaces are *not* encoded as a bit vector (e.g., a bit
per address space) but as a small integer indicating which address space is
selected (zero represents "no specific address space" which is sometimes
referred to as "the generic address space").  This small integer can be used as
an index in the array ``named_address_spaces``; this array is defined in
``targ_def.h`` and can be configured to describe the address spaces of a
particular target.

Variable length array types are a special case, because the number of elements
in the array is computed at run time, and so the dimension is represented as an
expression.  However, the type itself cannot point to the dimension expression,
since the former belongs to the file-scope memory region and the latter to a
function-scope memory region.  Instead, the flag ``has_assoc_vla_dimension`` is
set in the type entry, and the dimension expression is stored in an entry of
type ``a_vla_dimension``, which appears on a linked list pointed to by the
routine's IL scope entry (see ``find_vla_dimension`` in ``il.c``).  When the
bound of a VLA is unspecified (e.g., because it was declared with the ``[*]``
syntax), ``is_vla`` will be TRUE but ``has_assoc_vla_dimension`` will be FALSE.

.. _il-typerefs:

Typerefs
--------

Typerefs are used to record additional information for existing types:

* | typedefs giving a name for a type,
* | type qualifiers such as ``const`` and ``volatile``,
* | typeof (a GNU extension) and decltype (a C++11 feature) constructs,
* | specializations of alias templates,
* | type splices (a C++26 feature),
* | type-returning traits,
* | type-transforming attributes,
* | dependent type pack index specifiers, and
* | (when configured with DEFAULT_RECORD_FORM_OF_NAME_REFERENCE set to TRUE)
    lexical information such as nested name qualifiers and alternative template
    arguments.

Typeref type entries point to a typeref type supplement containing additional
information such as template argument lists, nested name qualifiers, and
operands of type operators.

For qualified pointer types, the typeref indicating the qualifier is on the
type of the pointer type: for ``int *const q`` the type is a const typeref
pointing to a pointer type entry, which points to an entry for ``int``.
(By comparison, ``const int *q`` produces a pointer type entry pointing to
a const typeref, which points to an entry for ``int``.)

It is often necessary for code that deals with types to access the target of a
typeref or chain of typerefs.  Various convenience functions are provided for
this purpose, including ``skip_typerefs``, ``skip_typerefs_not_typedefs``,
``skip_lexical_typerefs``, etc.

Class Types
-----------

Class types are ``class``, ``struct``, and ``union`` types [#f4]_.  Their
associated type entry always points to a class type supplement (conveniently
accessed with the macro ``class_type_supp``), which carries additional
information especially relevant to C++.

In C, a ``struct`` or ``union`` type is relatively simple, and is defined by a
list of ``a_field`` entries.  Each entry defines one field (member) of the
``struct`` or ``union``.  The offset of a field is given as the combination of
a byte offset (the ``offset`` field) and a bit offset (the
``offset_bit_remainder`` field).  The latter is nonzero only for bit fields
(i.e., when ``is_bit_field`` is TRUE); a bit field also has a nonzero
``bit_size``, and its type (an integral type) is significant only as an
indication of the base type specified in the source program.  Enum types and
integral types smaller than ``int`` can appear as the base type for a bit
field.  A separate flag ``bit_field_is_signed`` indicates whether or not the
bit field is signed.  Unnamed fields in the source program, which affect the
offsets of the fields that follow them (i.e., they introduce gaps), also appear
in the list of fields for the ``struct``.

An associated scope is defined for a ``struct`` or ``union`` in C even though
the C language doesn't really associate a scope with those constructs.  The
scope is used only to represent the parent of the type's fields.  The type does
have a class type supplement, but that's mostly necessary so that the pointer
to the associated scope can be recorded; the rest of the class type supplement
is irrelevant in C.

In C++, the fields are the nonstatic data members of the class, and the class
type supplement gives additional information about the class:

* | A list of the base classes of the class (both direct and indirect).
* | A list of the ``using``-declarations (and access adjustments, from access
    declarations), of friend classes and functions, and of classes that have
    befriended the class.  These are of interest to a back end only for
    generating symbolic debugging information.  Note that the friendship
    information is available in normal and reversed forms: the "befriending"
    information is backwards from the source form -- it appears on the class
    that is befriended, not the one that granted the friendship with a
    ``friend`` declaration.
* | A scope entry for the class, which contains the definitions of the static
    data members (as variables) and member functions (as routines) of the
    class.  The scope is present only if the class has been defined.

The nonstatic data members are listed in declaration order, which (with the
default class allocation scheme) is the same as allocation order.  Members of
base classes are not listed in the derived classes, even though they are in
some sense present by inheritance.

Base Class Entries
^^^^^^^^^^^^^^^^^^

A base class entry describes a specific base class of a specific derived class.
It's more than just the type of a base class -- it describes how that class
type relates to the derived class:

* | Where the storage for the base class appears in the derived class.
* | The derivation path from the derived class to the base class; for virtual
    base classes there may be more than one such path.
* | The type of derivation, e.g., is the base class a virtual base class?
* | The list of virtual functions of the base class that are overridden in the
    derived class.  (This information is useful for generating virtual function
    tables.)

Base class entries are used for indirect base classes (i.e., base classes of
base classes) as well as for direct base classes.

The base classes list for a given class lists all of the base classes of the
class all the way down, in depth-first left-to-right order (i.e., an indirect
base class precedes the direct base class derived from it; not coincidentally,
this is the order defined by the language as initialization order.) A
nonvirtual base class can appear more than once on the list -- it is marked as
ambiguous -- but a virtual base class will appear only once, though it may have
more than one derivation path.

Base class entries are used in the IL wherever it is necessary to provide an
unambiguous specification of a base class instance.

Class Scopes
^^^^^^^^^^^^

If a class is defined, its class type supplement will point to a scope entry
that defines the contents of the class (except the nonstatic data members,
which are provided by the field list of the class type).  The class scope will
contain:

* | Constants, for class constants (an extension).
* | Types, for types defined within the class (including nested classes).
* | Variables, for static data members and instances of variable templates.
* | Routines, for member functions, including static and non-static, virtual
    and non-virtual, inline and non-inline, constructors, destructors, operator
    functions, and conversion functions, but not friend functions.  The routine
    entries indicate any special attributes of each function, but the routine
    list is just a list in declaration order.  It's not broken into sections or
    indexed in any way.  The only way to find all virtual functions, for
    example, is to go through the list and examine each routine to see if it is
    virtual.

Class Member Declaration Lists
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The lists described above group the members of a class by category, and none of
them accounts for constructs like a ``static_assert`` declaration, which
declares nothing.  When ``MAINTAIN_CLASS_MEMBER_LIST`` is configured to TRUE,
the class type supplement of every defined class also points to the
``member_declarations`` list, which has one ``an_il_entity_list_entry`` per
declaration that appeared in the body of the class, in the order in which the
declarations appeared.  For example, the list built for

.. code:: c++

  struct S {
    static_assert(sizeof(int) == 4);
    int a, b;
    template <class T> void f(T);
    friend void g();
    enum E { e1, e2 };
  };

identifies, in order, a static assertion, a field (``a``), a field (``b``), a
template, a routine, and a type.

Note that:

* | A friend declaration that appears in the body is included even though the
    entity it declares is not a member of the class.
* | Entities that are not explicitly declared in the body are not included:
    that is the case for members that the front end generates (e.g., an
    implicitly-declared default constructor) and for template instantiations
    that are triggered while the body is being processed.  A member that is
    explicitly defaulted or deleted is declared, and is therefore included.
* | An anonymous union is represented by the entry for its type; no entry is
    created for the unnamed field through which the members of the union are
    accessed (that field can be reached through the
    ``anonymous_union_field`` pointer of the class type supplement).
* | The closure class of a lambda expression that appears in the body (e.g.,
    in a default member initializer) is not included; that class is reached
    through the ``a_lambda`` entry for the expression.
* | An enumerator is not included because the declaration of its enumeration
    type is; the enumerators can be reached through that type.
* | A member template is represented by the entry for its template, not by an
    entry for the prototype instantiation of the templated entity (see
    :ref:`templates-in-the-il`).
* | A *using-declaration* that names an overload set (or, with ``using
    enum``, an enumeration) produces several ``a_using_decl`` entries but only
    one entry on the list.
* | An entity that is declared more than once in the body is represented by one
    entry per declaration.  For example, a nested class that is first declared
    and later defined in the same body appears twice.

The list of an instantiated class template specialization describes the
declarations of the instantiated body.

Class Layout
^^^^^^^^^^^^

Class layout is done by routines in ``layout.c``, and the decisions made are
recorded in the IL in the form of offsets, sizes, and alignments.  The class
layout can be configured in various ways, or can be replaced in its entirety.
Class layout decides on:

* | Offsets of nonstatic data members (fields) within the class.
* | Offsets of base classes within the class.  For virtual base classes,
    offsets of the information that allows access to the virtual base classes
    (typically, a pointer to the virtual base class).
* | Offsets of virtual function information (typically, a pointer to a virtual
    function table).
* | Ordering of base classes and members, and insertion of necessary alignment
    between them.

Virtual function table pointers and virtual base class pointers can be shared
with base classes.  That is, space for such pointers need not be allocated in a
derived class if they are present in a base class; the derived class can use
the pointer in the base class.

There are many configuration macros that control class layout.  See
``targ_def.h``.

C++/CLI Types
^^^^^^^^^^^^^

In addition to the standard types, C++/CLI has

* | handle and tracking reference types, which are represented as
    ``tk_pointer`` types, but aren't considered to be standard pointers or
    standard references;
* | ``interior_ptr`` and ``pin_ptr`` types, which are variants of standard
    pointer types with certain added attributes and restrictions (they only
    appear as the types of local variables, parameters, or return types);
* | managed class types (``ref``\ ``struct``, ``ref``\ ``class``, ``value``\
    ``class``, ``interface``\ ``class``), which are represented like standard
    class types, with a value in the ``cli_class_type_kind`` field that
    indicates the kind of managed class;
* | ``enum``\ ``class`` types, which are similar to C++11 scoped enum types and
    similarly represented;
* | CLI array types, which are represented as managed class types tagged with
    the ``is_cli_array`` field; and
* | delegate types, which are represented as managed class types tagged with
    the ``is_delegate_class`` field.

Fundamental types usually have corresponding value class types (e.g., ``int``
corresponds to the ``System::Int32`` value class type).  In general, types
written in the source in either form are converted to the fundamental type
version.  In cases where a managed type is required, the fundamental type is
converted to the value class, so for example "``int ^``" becomes "handle to
``System::Int32``" as soon as it is created.  Otherwise, conversions between
the two kinds of types are always indicated explicitly.  In particular, in
expressions a conversion from one form to the other will be indicated
explicitly, e.g., with a box or unbox operator.

Namespaces
==========

Namespaces are represented by entries of type ``a_namespace``.  Such an entry
is used for a namespace definition, in which case it points to a scope entry
that describes the contents of the namespace, and also for a namespace alias,
in which case it points to another namespace entry.

Entries of type ``a_using_decl`` describe ``using-directive``\ s and
``using-declaration``\ s that appear in the source program.  This
information is not needed for code generation, but may be useful in the
generation of symbolic debug information or for source-to-source
translation.

Constants
=========

``a_constant`` describes a target machine constant, which can be an integer, a
string, a fixed-point constant, a floating-point constant, a complex or
imaginary constant, an address, or a pointer-to-member.  Nontype template
parameters and the UPC identifiers ``THREADS`` and ``MYTHREAD`` are also
represented using ``a_constant`` entries.  The constants are kept in target
form, and should not be manipulated without using the target-specific routines
for that purpose.  Fixed-point constants are stored as ``a_fixed_point_value``\
s.  Floating-point constants are stored as ``an_internal_float_value``.  String
constants are stored as a string of bytes in the target character set with an
associated length (to avoid any question about extra or missing null characters
in the string).  The text of string constants is always allocated in the file
scope memory region to avoid some memory region problems.

Integers are usually maintained in a host large integer
(``a_host_large_integer``/``a_host_large_unsigned``) regardless of the size
or signedness of their actual integral type.  They are always properly
masked and sign extended.  If a host large integer is insufficiently large,
the front end can be configured to use a more general (but computationally
less efficient) representation of integer values (see the configuration
macro ``INTEGER_VALUE_REPR_IS_A_HOST_INTEGER``).  In either case,
``an_integer_value`` is the type to use internally to represent an integer
constant.

Constants are also used as initial values of variables.  When those variables
are aggregates (arrays or classes), some special kinds of constant entries are
used to represent aggregates of constants and repetition of constants within
those aggregates.  Such constants are acting more as a structuring entity than
as simple constants; it's just convenient to use the ``a_constant`` entry for
that function as well.  Getting even farther afield, there is even a constant
kind (``ck_dynamic_init``) that indicates an element in an aggregate constant
that is in fact not a constant at all, and must be computed at run time and
placed at that position in the aggregate.  The structure of the aggregate
constant will match the structure of the variable being initialized.  For
example, the initial value for a variable whose type is "array of 4 structures
containing 2 integer fields" will be an aggregate constant containing a list of
four constants, each of those an aggregate constant containing a list of two
integer constants.  Aggregate constants need not fully initialize an aggregate
(i.e., there may be fewer constants than there are members of the aggregate);
any members not explicitly initialized are implicitly initialized to zero (even
when the variable being initialized is non-static).

Most constants, specifically those used in expressions, are shared and reused,
so that only one constant ``0``, for example, need be allocated.  Constants
used as constant initial values, however, are (and must be) unshared.  [#f5]_
Since the aggregating kinds of constant entries are only used in initial
values, it follows that they are always unshared.  In ``pcc`` mode, string
constants are considered writable and are not shared.

In some cases, a program may include a cast of a constant value to another
type, and the result (clearly constant) may be unrepresentable in any of the
normal forms.  An example is an integer constant cast to a pointer type.  For
these situations, the ``a_constant`` entry has the ``implicit_cast`` field.
When set, it indicates that the value of the overall constant is to be the
constant value cast to the type indicated by the ``type`` field.  The back end
must determine the exact value to be used.

Address constants are used to refer to the addresses of variables,
functions, string literals, etc.  The offset in address constants is
expressed in bytes and is not scaled by the size of the object pointed to
(unlike in the C and C++ languages).  Address constants are only used for
addresses that are truly constant, not, for example, for the addresses of
``auto`` variables.  Note, however, that the constant value may not be
known until link time, i.e., it might not be a compile-time constant.

Pointer-to-member constants represent the location of a particular data member
or member function of a class within an arbitrary instance of that class, i.e.,
what is written in source as something like ``&A::xx`` (with ``xx`` a nonstatic
member of ``A``).  They are defined in a high-level form rather than (for
example) as an offset within the class, to leave it to IL lowering or the back
end to choose an implementation for pointers to members.

Wide character constants are represented as integral constants.  Wide string
literals are represented as string constants.  There are routines in
``literals.c`` that do the necessary packing.

When character or string literals contain a universal-character-name (e.g.,
``\u00d6``), the UCN value is converted from Unicode to the appropriate
character value or multibyte character sequence and stored in the string.

In C++/CLI, string literals start out as standard C++ string constants, and are
converted to C++/CLI strings if the context requires it.  So, for example, if a
standard string literal is passed as an argument to a function that takes a
parameter of type "``System::String``\ ``^``", the string will be converted to
a C++/CLI string.  This is done by setting the constant type to "handle to
``System::String``" without setting the ``implicit_cast`` flag.  There is no
conversion of the characters in the string to Unicode (or in any other way), in
spite of what the ECMA standard seems to require.  Wide string literals can
also be converted to C++/CLI strings in this way, and in that case the string
contents are treated as Unicode (as they always are in Microsoft mode, and
therefore in C++/CLI mode; but still no conversion is done).  Having C++/CLI
strings represented as constants is a little strange, since ultimately they
have to result in creation of a class object on the heap, and use of its --
non-constant -- address; however, MSVC treats such strings as constants, so
emulating the Microsoft compiler is easier when they are represented as
constants.  It is left to back ends to transform these constants into
executable code as needed.

The front end generally attempts to fold constant-expressions into a single
``a_constant`` node.  The expression that led to the constant is also recorded
in the ``expr`` field in the ``a_constant`` entry.  If this field is nonnull,
it points to the expression tree which produced the constant when folded; the
resulting constant is not shareable.

Variables
=========

``a_variable`` describes a variable, parameter, static data member, instance of
a variable template, temporary, or structured binding.  It indicates the type
and storage class.  The storage class can be ``sc_auto``, ``sc_register``,
``sc_static``, ``sc_extern`` (meaning a reference to an external variable
defined in another compilation), or ``sc_unspecified`` (meaning a definition of
a variable visible to other compilations).  The storage class is standardized
during declaration processing.  Parameter variables can only have storage
classes of ``sc_auto`` or ``sc_register``.  File-scope and namespace scope
variables, instances of variable templates, and static data members can only
have storage classes of ``sc_static``, ``sc_extern``, or ``sc_unspecified``.
Function- and block-local variables can only have storage classes of
``sc_auto``, ``sc_register``, or ``sc_static``.

Variables declared with a named-register storage class (an Embedded C
extension) have storage class ``sc_unspecified`` (not ``sc_register``) but also
have the flag ``has_named_register_storage_class`` set to TRUE.  The register
is described using a small integer stored in the variant field
``asm_name_or_reg.id``, which can be used to index the array
``named_register_storage_classes``.  This array is defined in ``targ_def.h``
and can be customized to describe the named-register storage classes available
on a particular target.

If the variable has an initial value, the field ``init_kind`` indicates whether
the variable should be statically initialized to a constant value (simple or
aggregate) or dynamically initialized to a constant or non-constant value
(simple or aggregate).  In the dynamic initialization case, the variable points
to a dynamic initialization entry that indicates the initialization to be done.
(More on this below.) Note that static initialization is never used for
non-static local variables, even ones initialized with a constant value: static
initialization initializes the entity once, at program startup time, whereas an
automatic local variable must be initialized each time the function is invoked
and the variable declaration is reached.  Structured bindings (a C++17 feature)
that refer to array elements or to fields are represented as variables, but
they are really aliases for simple expressions referring to parts of the
associated (unnamed) container variable.  These bindings have an
``initk_binding`` initializer kind and point to the aliased expression via
``initializer.binding``.  Structured bindings that bind to a
``std::tuple``-like container are ordinary reference variables with
``initk_dynamic`` initializers.

An uninitialized static variable is considered to be implicitly initialized to
zero.  There is also an ``initk_zero`` initialization kind, which can be used
to explicitly initialize a variable to zero, statically or dynamically.  On an
external variable, such initialization has the added function of indicating
that the variable is a true definition rather than a tentative definition.

If the address of the variable has been taken anywhere (implicitly by
conversion of an array to a pointer or in binding of a reference, or explicitly
by use of the ``&`` operator), the flag ``address_taken`` will be set.

If a variable is a local static variable of a function,
``referenced_non_locally`` is TRUE if the variable is referenced from a
different function, specifically from a member function of a local class.  This
may have an effect on aliasing analysis.

Local static variables create a memory-region difficulty: the variable entry
must be allocated in the file scope memory region, because the variable can be
referenced from the bodies of member functions of local classes.  Dynamic
initialization of the variable, however, can be based on an expression
referencing a local (nonstatic) variable of the function, and therefore the
dynamic initialization must be allocated in the function scope memory region.
The ``a_local_static_variable_init`` entry solves this problem.  When a local
static variable has initialization that must be allocated in the function scope
memory region, the variable ``init_kind`` is set to ``initk_function_local``.
This is an indication that the variable is initialized, but details of the
initialization are not available in the file scope.  In the function or block
scope in which the variable is defined, there will be
``a_local_static_variable_init`` entry on the ``local_static_variable_inits``
list of the scope.  That entry will point to the variable, and give the
initialization details.  Conceptually, the initialization information in the
local initialization entry is part of the variable information (it provides the
true values for the ``init_kind`` and ``initializer`` fields of the variable);
it's in a different place because of memory region issues.  The routine
``get_variable_initializer`` can be used to fetch the complete initializer
information for a variable.  It will search for and retrieve the function-local
information when necessary.  Local static variables with initial values that
are simple (non-aggregate) constants always use ``initk_static`` initialization
rather than the ``initk_function_local`` form.  This is done so that the values
of integral constant variables can be retrieved as constants while compiling
local classes.  ``initk_function_local`` is used for dynamic initialization and
for aggregate constant initialization.

Temporary variables are generated by IL lowering, for class temporaries and
when initializing references.  Temporary variables are also generated for
anonymous unions; see :ref:`anonymous-unions` for more information.

.. _il-dynamic-init:

Dynamic Initialization
======================

A dynamic initialization entry (type ``a_dynamic_init``) is used to represent
initializations that require the execution of code at runtime, i.e., that
cannot be done as static initializations to a constant.

The ``kind`` field of the entry indicates the kind of initialization to be
done:

* | Initialization to a constant.  Note that this is different than static
    initialization to a constant, in that it gets done at a particular time and
    possibly more than once.  The constant here is unshared and may be an
    aggregate (one that is completely constant).
* | Initialization to an expression.  An expression is evaluated and its value
    is the initial value.
* | Initialization by a constructor.  A constructor routine is called to
    initialize the entity.
* | Initialization by a routine returning a class object.  A routine is called
    and the value it returns (by calling a copy constructor) initializes the
    entity.

A dynamic initialization entry can also indicate a destructor that must be
called to destroy the entity initialized.  [#f6]_ Such dynamic
initialization entries will be linked onto the destructions list of an
object lifetime entry.  The destruction is to be done any time the region
of the object lifetime is exited, and IL lowering generates the appropriate
destructions.  See :ref:`object-lifetimes`.

A dynamic initialization entry indicates *what* the initialization is to
do, but it does not indicate *where* the initialization is to be done
(except if the entity being initialized is a complete variable), and it
does not indicate *when* the initialization is to be done.  Dynamic
initialization entries are used in several cases, and the context in each
case provides the additional information needed:

* | For initialization of file-scope variables, instances of variable
    templates, or static data members, the variable points to the dynamic
    initialization entry, and the ``dynamic_inits`` list from the scope entry
    for the file scope indicates the order in which the initializations should
    be performed.  Variables from namespaces also appear on this same list,
    since their initializations are intermixed with those of the file-scope
    variables.
* | For initialization of function and block variables, the variable points to
    the dynamic initialization entry, and there is an ``stmk_init`` statement
    in the executable code of the function that points to the same dynamic
    initialization entry and indicates the point at which the initialization
    should be done.  An ``stmk_init`` statement can appear in the same block as
    the variable declaration or in a nested block.  If the variable is a local
    static variable, the initialization is to be done only the first time the
    ``stmk_init`` is reached, and corresponding end-of-program destruction must
    be done only if the initialization was done.  Note that if a local variable
    uses dynamic initialization it *must* have an associated ``stmk_init``
    statement.
* | For initialization of temporaries in the middle of expressions, an
    ``enk_temp_init`` node points to a dynamic initialization entry that
    indicates the initialization to be done.
* | For initialization of storage allocated by ``new``, an ``enk_new_delete``
    node points to a dynamic initialization entry that indicates the
    initialization to be done.  These dynamic initialization entries never
    indicate destruction.
* | For destruction of storage in a ``delete``, an ``enk_new_delete`` node
    points to a dynamic initialization entry that indicates the destruction to
    be done.  These dynamic initialization entries never indicate
    initialization.
* | For initialization of members in a constructor-initializer (either
    user-written or front-end-generated), an entry of type
    ``a_constructor_init`` indicates the member or base class to initialize and
    points to a dynamic initialization entry to indicate what to do.  The order
    of the ``a_constructor_init`` entries gives the order in which the
    initializations should be done.  These dynamic initialization entries never
    indicate destruction (except when exceptions are enabled, in which case the
    destructions indicated are to be done if an exception is thrown before the
    constructor completes).
* | Similarly, for destruction of members in a destructor (which can only be
    front-end-generated), an entry of type ``a_constructor_init`` indicates the
    member or base class to destroy and points to a dynamic initialization
    entry to indicate what to do.  The order of the ``a_constructor_init``
    entries gives the order in which the destruction should be done (i.e., if
    one looks at a constructor and destructor for a class, the entries on the
    destructor will be in the reverse order of those on the constructor).
    These dynamic initialization entries never indicate initialization.
* | For non-static data member initializers (NSDMIs), more recently renamed
    as default member initializers (DMIs), the field entry ``initializer``
    field points to a dynamic initialization entry giving the initializer.
    If such an initialization is not overridden in a given constructor, the
    ``a_constructor_init`` entry for the field will have
    ``use_field_initializer`` TRUE to indicate that the initial value comes
    from the field initializer.
* | For dynamic initialization of entities within an aggregate, for example

  .. code:: c++

     int i[3] = {1, 2, j+k};

  | dynamic initialization entries at two levels are needed.  The variable
    ``i`` will point to a dynamic initialization entry that indicates the
    overall initialization.  Because the initialization is an aggregate
    initialization containing a non-constant value, the dynamic initialization
    will have the kind ``dik_nonconstant_aggregate`` and will point to a
    ``ck_aggregate`` constant.  That constant points to a list of the constants
    in the aggregate, beginning with the constants ``1`` and ``2``.  Following
    that is another constant entry of kind ``ck_dynamic_init`` which indicates
    "this position in the aggregate is not really a constant; here's what needs
    to be done to compute it" and points to another dynamic initialization
    entry, this one of kind ``dik_expression`` and indicating that the value to
    use for the initialization is the expression ``j+k``.
  |
  | Within the aggregate constant list, ``ck_init_repeat`` constants are used
    to repeat a particular ``ck_dynamic_init`` initialization a specified
    number of times.  This is needed to do constructor initialization of arrays
    of classes.
  |
  | In this aggregate case, the top-level dynamic initialization entry serves
    as more of an aggregating construct than as a specification of a dynamic
    initialization.  It shouldn't come as much of a surprise, then, to learn
    that to specify only destruction for an aggregate, one uses a top-level
    ``dik_nonconstant_aggregate`` dynamic initialization entry as is done in
    the initialization case, and the destruction is specified in the
    lower-level dynamic initialization entry.  The troublesome thing about this
    form is that one cannot tell just by looking at the top-level dynamic
    initialization whether the overall construct does initialization,
    destruction, or both.  One must find and examine the lower-level dynamic
    initialization entry.
* | For returns from routines that return a class by value, the ``stmk_return``
    statement points to a dynamic initialization that indicates the way to
    create the class being passed back to the caller.
* | For expressions that are operands of ``throw``, the ``throw`` expression
    node points to a dynamic initialization that indicates the way to copy the
    value for use by the runtime.
* | For parameters of ``catch`` clauses, the catch clause points to a dynamic
    initialization that describes the way to initialize the parameter from the
    runtime's copy of the thrown object.

IL lowering rewrites all dynamic initializations that are not valid in C, and
therefore constructor and destructor calls and non-constant aggregates do not
appear in the lowered IL.

.. _object-lifetimes:

Object Lifetimes
================

In C++, class types can have associated destructors, which are called to
destroy objects of the class type at the ends of their lifetimes.  These calls
are implicit: the programmer does not write them, and the compiler generates
them in the appropriate places.  For example, on a ``goto`` out of a block
containing a class variable, the destructor for the variable must be called.
If there are several possible ways to exit the block (e.g., flowing off the
end, ``goto``\ s, exceptions), the destructor must be called in each case.

The IL does not make the needed destructor calls explicit (in fact, for
exception handling, the needed destructor calls would be done by the runtime,
so there's no way to put them in the IL without dictating a particular
implementation of exception handling).  Instead, it gives information about
needed destructions and the associated object lifetimes in such a way that a
back end can easily generate the required destructor calls.  (IL lowering puts
in those calls, so a back end working from the lowered IL need not deal with
object lifetimes.)

The IL concept of an "object lifetime" is modeled on a source language concept,
the idea that an object's lifetime (the time during which it has a meaningful
value) might be different than the lifetime of the storage for the object.
That is, the object is constructed, used, and destroyed, and then the storage
that contained the object might remain around for some time longer, but it no
longer contains meaningful information and is no longer an object.

It should be noted that from an IL point of view, the concept of object
lifetime is only applied to objects of class types having destructors.  From a
strict language point of view, other kinds of objects do have object lifetimes,
but those object lifetimes are not very interesting because they always match
the lifetime of the object storage; more precisely, in the absence of a
destructor there is no way to tell the difference between an object whose
lifetime has ended and one that is still active, as long as the storage remains
allocated to the entity.  Therefore there is no benefit in associating IL
object lifetimes with entities that do not need destruction, and there *is* a
benefit -- space savings -- in not doing so.  Note that this means that IL for
C programs, which by definition involve no constructors, never includes object
lifetimes.

The IL entry ``an_object_lifetime`` represents and defines an object lifetime.
There are several kinds of object lifetimes, each associated with a certain
kind of entity in the IL: [#f7]_

* | A static object lifetime, which lasts until the termination of the program.
    For packaging reasons, there is a global static object lifetime and
    possibly several function static object lifetimes (one for each function
    that contains destructible local static entities).  Such lifetimes are
    bound to the file scope and function scopes.
* | A block object lifetime, which lasts until the termination of a function or
    block scope.  Such a lifetime is bound to a function or block scope.  There
    is also a variant of this kind of lifetime, referred to as a
    block-after-label lifetime, that lasts from a label or (in
    long-lifetime-temporaries mode) a switch case or default label to the end
    of the scope.  The block-after-label lifetime points to a statement that is
    or contains a label or to a ``stmk_switch_case`` statement, respectively.
* | A try-block object lifetime, which surrounds the execution of a ``try``
    statement and its associated ``catch`` clauses, and is bound to a
    try-supplement.
* | An expression temporary object lifetime, which lasts until the end of a
    full expression, and is bound to an expression node of kind
    ``enk_object_lifetime``, or to a dynamic initialization entry.

Each dynamic initialization in the IL that requires end-of-lifetime destruction
is placed on the linked list of destructions attached to the proper object
lifetime entry.  The destructions in a given lifetime are linked together by
the ``next_in_destruction_list`` pointer, in the order in which they should be
destroyed (that is, the reverse of the order of construction).  The
``destructions`` pointer in the object lifetime entry points to the first
initialization on the list (the last one constructed and the first one to be
destroyed).

Not all dynamic initializations are placed on a lifetime list.  Initializations
of nonclass objects, or objects of class types that have no destructors,
clearly do not need to go on lifetime lists.  Even initializations of objects
of class types with destructors are put on lifetime lists only if the
destruction of the object is automatic at the end of the lifetime, e.g., not
for ``new`` (the programmer controls the time of destruction) or ``return`` by
constructor (the destruction is done by the caller, not the subroutine).

Each object lifetime has a ``parent_lifetime`` pointer, and also a
``parent_destruction_sublist`` pointer giving the position of the child
lifetime relative to the destructions in the parent.  This makes a tree
structure that one can enter at some inner lifetime and follow upward.  The
path upward from a given starting point is referred to as the "cleanup chain"
starting at that point.  For example:

.. code:: c++

   struct A { A(); ~A(); };
   void () {        // Lifetime 1 associated with this block
     A x;
     A y;
     {              // Lifetime 2 associated with this block
       A xx;
       // Position R
       A yy;
     }
     A z;
   }

Lifetime 1 has the destructions list ``(z, y, x)`` and lifetime 2 has the
destructions list ``(yy, xx)``.  Lifetime 2's ``parent_lifetime`` pointer
points to lifetime 1, and its ``parent_destruction_sublist`` pointer points to
the initialization entry for ``y``.

One can see that if a ``return`` statement appeared at position R above, one
would want to destroy ``xx`` from the inner lifetime and ``y`` and ``x`` from
the outer lifetime.  How would a back end figure that out? Well, as the back
end processes the IL, it would keep track of the latest
initialization-with-destruction encountered, i.e., the one for ``xx`` when at
position R.  The pointer to that initialization defines the cleanup chain that
applies at position R.  Starting with that initialization, the back end would
follow the ``next_in_destruction`` list to the end, generating destructions for
the entities on that list.  Then, using the ``lifetime`` pointer in the last
dynamic initialization and the ``parent_lifetime`` pointer in the lifetime
pointed to, it would go to the parent lifetime, entering its destructions list
at the point indicated by ``parent_destruction_sublist``.  It would then
generate destructions for the entities on that list.  The process would
continue upward through parent lifetimes until the lifetime for the function is
processed.

Generating destructions at a ``goto`` is almost as easy.  The ``goto``
statement has a ``common_lifetime`` field set by the front end.  It indicates
the first object lifetime that the ``goto`` and the label have in common, that
is, the innermost object lifetime that appears on the cleanup chains for both
the goto and the label.  The destructions for the ``goto`` are generated by
working upward through the cleanup chain, as for the ``return`` case, except
that processing stops when the common lifetime is reached (before generating
any destructions in that lifetime; if the ``goto`` is directly in the common
lifetime, then no destructions are generated).

When labels appear, and are followed by declarations for destructible objects,
a new block-after-label lifetime is begun at the label.  This is necessary for
backwards ``goto``\ s:

.. code:: c++

   struct A { A(); ~A(); };
   void () {     // Lifetime 1 begins
     A x;
   L:;           // Lifetime 2 begins after label
     A y;
     if (i) goto L;
   }

The destruction of ``x`` will be in lifetime 1, and the destruction of ``y``
will be in lifetime 2.  The ``goto`` is in lifetime 2, and the label is in
lifetime 1 (the parent of lifetime 2), so the common lifetime for the ``goto``
is lifetime 1.  The cleanup code at the ``goto`` therefore destroys ``y`` as it
exits lifetime 2, but it does not destroy ``x``.

Switch case labels are treated like other labels and can begin new object
lifetimes if necessary.  The labels generated for ``break`` and ``continue``
statements do not ever begin new lifetimes.

Destructible temporaries are treated much the same way as destructible
variables, and their initializations are likewise placed on lifetime lists.  In
its choice of which lifetime to use in each case, the front end represents its
decisions on the lifetimes of temporaries.  For example, when a reference
variable is bound to a temporary, the C++ language dictates that the lifetime
of the temporary is extended to match the lifetime of the reference (e.g., for
a static reference the temporary lasts until the end of the program).  The
front end would represent this in the IL by placing the temporary on the same
lifetime as the reference variable.

In the modern C++ language, normal temporaries last until the end of the full
expression within which they occur.  In the front end, this is represented by
placing an expr-temporary lifetime around the full expression and placing the
destructions of any temporaries therein.  In older dialects of C++, however,
including the dialect implemented by cfront, temporaries lasted until end of
scope.  This mode is implemented by the EDG C++ front end in cfront
compatibility mode and when ``--long_lifetime_temps`` is specified.  To be more
precise, in long-lifetime-temporaries mode temporaries survive until end of
scope or until a label, and they are limited to a single statement when the
statement is a conditional dependent statement, or to a full expression when
the expression is the tested expression in a conditional or loop statement, or
the initializer for a local static variable.  Since long-lifetime temporaries
are destroyed at a label, when walking a cleanup chain and going from a
block-after-label lifetime to its parent one should ignore destructions for
temporaries until one reaches and exits from a block lifetime (all temporaries
in the block lifetime and any intervening block-after-label lifetimes were
destroyed at some label, and are no longer active).  Because of the destruction
of temporaries at labels, in this mode block-after-label lifetimes are required
at some labels even when there are no destructible objects created after the
label.

In addition to its use in generating destructions on programmed exit from
lifetimes, the block object lifetime information is also intended to be useful
in generating the cleanup description information needed by the runtime support
for exception handling.  With the default configuration, IL lowering will use
the object lifetime information to generate data structures that support EDG's
implementation of exception handling.  Those who wish to create a
higher-performance version of exception handling may find it useful to turn off
lowering of exception handling features and to use the object lifetime
information directly in the back end to generate the required cleanup
information.  In either case, because the object lifetime information is used
to generate exception handling information, it includes some information beyond
what is required to generate cleanup on block exits:

* | There is information about the mem-initializers (both user-written and
    front-end-generated) in constructors, and about the similar
    (front-end-generated) data structure in destructors.
* | For each ``new`` that does allocation inline (rather than letting a
    constructor do it), a dynamic initialization entry is generated to indicate
    the freeing of the storage to be done if an exception is thrown before the
    storage is fully initialized.
* | For local static variables, a block object lifetime is placed around the
    entire initialization and bound to the local static variable initialization
    IL entry.  Exiting this lifetime via an exception causes the runtime to
    reset the state so that the local variable is marked as uninitialized.  If
    its declaration is encountered again on a subsequent call, the
    initialization will be attempted anew.
* | For aggregate classes or arrays initialized by a brace-enclosed list,
    individual sub-initializations will indicate destructions to be done if an
    exception is thrown before the complete aggregate is initialized.  Once the
    aggregate has been completely initialized, these destructions to undo
    partial construction no longer apply.

One of the unfortunate consequences of using dynamic initialization entries to
represent destructions is that the contextual nature of dynamic initialization
entries (they don't indicate the entity to be initialized; it's implied by the
context) -- which is an advantage when the entries are used to do
initializations -- is somewhat of a disadvantage when the entries are used to
do destructions.  Specifically, unless one saves some information in the
dynamic initialization entry at the time it is processed as an initialization,
it is very hard to know what to destroy when one processes it as a destruction.
However, it's not hard to solve this problem from a practical point of view:
one just adds a field to the dynamic initialization entry to point to
back-end-specific information recorded at the time that the initialization is
processed, and that information is then available, and in just the right form,
when the destruction is processed.  That's the technique used by IL lowering.

Routines
========

``a_routine`` describes a function.  It indicates the type and storage class.
The storage class can be ``sc_static``, ``sc_extern`` (meaning a reference to a
function defined in another compilation), or ``sc_unspecified`` (meaning a
definition of a function visible to other compilations).  The storage class is
standardized during declaration processing.  A function with a body will have
storage class ``sc_unspecified`` or ``sc_static``; a function without a body
will have storage class ``sc_extern``.

The type of a function is always a function type, which describes the
external interface to the function: its return type, its parameter types,
etc.  A nonstatic member function will also have an implicit "``this``"
parameter, with a type of ``const`` pointer-to (possibly qualified) class.
The parameter types might have associated default argument value
expressions, although that information is useful to a back end only for
generation of symbolic debugging information, since calls of the routine
will have the default argument values supplied.

The routine entry also indicates whether or not the routine is inline, virtual,
and/or front-end-generated.  (The front end generates routines for constructors
and destructors, assignment operator functions, and, when IL lowering is done,
for global initialization and termination routines, runtime routines, and
wrappers for constructors with default arguments.) The ``special_kind`` field
indicates whether or not the function is a C++ special function such as a
constructor.  If the ``special_kind`` indicates that the routine is an operator
function, ``opname_kind`` indicates the kind of operator function it is.  The
``special_kind`` is significant to a back end using the unlowered C++
intermediate code in that wrapper code needs to be generated for constructors
and destructors.  Other than that, ``special_kind`` and ``opname_kind`` are
mostly of interest for generating symbolic debugging information and mangled
names.

The routine entry points to a list of classes that have befriended the
function.  Those classes will have the function on their friend lists.

If the function is a virtual function, the routine entry indicates a virtual
function number assigned to the routine.  Typically, this will be used as the
function's index into a virtual function table, but since the unlowered C++ IL
leaves virtual function access as an abstraction, it's only a helpful
identifying number at that level.

If the function has a body, the routine entry indicates the function
definition number and the memory region that contains the function
definition.  By indexing through ``il_header.function_def_table``, one can
get to the scope entry for the function (in the proper function scope
memory region).  That scope entry points to the statements and local
declarations of the function.  It also points to variables for the
parameters of the function, including a "``this``" parameter variable if
necessary.  The parameter and "``this``" variables will match the parameter
and "``this``" types in the function type.  (In non-prototyped functions, the
function type parameters might be promoted versions of the parameter
variable types.) For parameters that are passed using a copy constructor
(indicated by a flag in the parameter type entry), a back end using the
unlowered C++ IL may have to expect to receive the address of the class
entity rather than its value, and to do implicit indirections on the
references to the parameter.

The ``address_taken`` flag in the routine entry will be set if the address of
the function has been taken.  Merely calling the function does not cause the
``address_taken`` flag to be set, even though the IL for a call refers to the
routine address.

An exception specification that appears on a function declaration is
represented by an entry of type ``an_exception_specification``, referred to
by means of the ``exception_specification`` field of the routine type
supplement.  When the function is declared with no exception specification
(meaning that *any* exception might be thrown) the
``exception_specification`` pointer will be NULL.  The exception
specification entry representing a dynamic exception specification has a
pointer to a linked list of entries of type
``an_exception_specification_type``, each of which indicates a type of
exception that will be thrown from the given routine; that pointer is NULL
to indicate that *no* exceptions will be thrown.  An exception
specification representing a ``noexcept`` specification points to an entry
of type ``a_constant`` representing its operand (or NULL if no operand is
specified).

When ``ASM_FUNCTION_ALLOWED`` is TRUE, ``asm`` functions are recognized and
passed uninterpreted to the back end.  They are represented in the IL like
ordinary functions, except that they have a storage class of ``sc_asm`` and the
function body (pointed to from the associated scope entry) is represented by an
``stmk_asm`` statement.  The asm entry to which the latter refers points is a
null-terminated string containing the text that appeared in the source;
comments are removed unless ``INCLUDE_COMMENTS_IN_ASM_FUNC_BODY`` is TRUE.

In order to save space in the ``a_routine`` structure, some (mostly
GNU-specific) fields with default values that are rarely changed are located in
the ``a_gnu_routine_supplement`` struct, which is pointed to by the
``gnu_extra_info`` field.  This supplement is only allocated when one of the
fields has a non-default value.

The GNU function multiversioning feature (available in GNU C++ modes where
``gnu_version`` >= 40800), allows for declarations/definitions of multiple
routines with the same signature but with different ``target`` attributes.  The
``target`` attributes specify which CPU architecture is applicable for each of
the functions in the group.  Internally, the entire group of functions is
represented by a "representative" function (whose ``is_representative`` field
is TRUE).  All target-specific functions are have the
``is_target_specific`` field set to TRUE and are queued on the
``mv_info.representative.targeted_versions`` list.  Although each of the
routines (both representative and target-specific) have associated symbols,
only the symbol for the representative routine is entered into the symbol
table.  This feature is controlled by the ``GNU_FUNCTION_MULTIVERSIONING``
configuration macro.

Constructors and Destructors
============================

Constructor and destructor routines must do additional processing beyond what
appears in the body of the routine.  Since this processing surrounds the code
in the definition of the routine, it is called "wrapper" code.

Some of the wrapper code is implicit: there must be code that initializes
the information in the class used to access virtual base classes and
virtual functions (typically, virtual base class pointers and virtual
function table pointers).  The wrapper code must also control whether or
not virtual base classes are initialized or destroyed (they should be for a
complete object, but not for a subobject).  If a ``new`` or ``delete``
operation can be folded into a constructor or destructor (a configuration
option, but always true if assignment to "``this``" is supported), the
wrapper code may have to provide for calling a ``new`` or ``delete``
routine.

The wrapper code must also do any necessary initialization or destruction for
base classes and members.  The required actions are described explicitly by a
list of ``a_constructor_init`` entries which indicate the initialization or
destruction to be done, in the correct order.  If the source program contains
constructor-initializer clauses, they will appear on the list in the right
positions.  For any base class or member that requires initialization and for
which the source does not provide an initializer, a default initializer is
generated.  In a front-end-generated copy constructor, the default
"initialization" will in fact be a copy.  In a destructor, there is no source
construct for indicating specific destruction for base classes and members, so
the list is always front-end-generated in its entirety.  It is also in the
correct order, i.e., the reverse of the order in the corresponding constructor
list.

IL lowering modifies constructor and destructors to provide all the necessary
wrapper code.

Calls of "destructors" for classes that do not have them or for simple types,
as in

.. code:: c++

   p->int::~int();

are rendered as ``eok_dot_vacuous_destructor_call`` or
``eok_points_to_vacuous_destructor_call`` expression nodes in the IL.  No
routine entry is created for the ersatz destructor.  Such nodes are rewritten
as casts to ``void`` in IL lowering.

Labels
======

``a_label`` is a declarative entity that describes a label (user-written or
front-end-generated) in a function.  It is pointed to by the ``goto`` and label
definition statements in the executable code of the function.

``asm`` Statements
==================

An ``asm`` statement (or declaration, whichever way you care to look at them)
can appear in two contexts:

* | Within the bodies of functions, where it is considered to be executable
    code and is represented by an ``stmk_asm`` statement pointing to
    ``an_asm_entry`` that gives the ``asm`` statement string.
* | At file or namespace scope, where it is considered to be a declaration and
    is represented as ``an_asm_entry`` on a list of such entries attached to
    the file or namespace scope ``a_scope`` entry.  If such ``asm``
    declarations are supposed to affect declarations that immediately follow
    them, the back end must consider the source positions of variables,
    routines, and ``asm`` entries and do a merge of those lists.

Statements
==========

``a_statement`` describes a statement, which can be an expression, an ``if``, a
``while``, a ``do``-``while``, a ``for``, a ``switch``, a ``goto``, a
``break``, a ``continue``, a label definition, a ``return``, a block (with or
without an associated scope), a ``try`` block, or a declaration statement.

The representation of ``switch`` statements (``stmk_switch``) matches the
language definition, but the ``case`` and ``default`` labels are represented
using a separate statement entry kind ``stmk_switch_case``.  Each
``stmk_switch_case`` entry points to an entry of type ``a_switch_case_entry``
(which describes the label's value, among other things).  An ``stmk_switch``
statement points to a list of all switch case entries associated with that
statement (via an entry of type ``a_switch_stmt_descr``).  A list of switch
case entries in sorted order of label value is usually also available (this
list excludes the ``default`` case), but not if any of the cases is
template-dependent.  ``break`` statements to transfer out of a ``switch``
statement are ``stmk_goto`` statements pointing to a label entry whose
``switch_break_label`` flag is TRUE.

The ``stmk_init`` statement indicates the point in a sequence of executable
statements where the dynamic initialization of a variable should be done.  It
points to the same dynamic initialization entry that the variable does.  This
is particularly useful in C++ where variables can be declared and initialized
in the middle of a block.

The ``stmk_block`` statement is used both for a simple compound statement and
for a block (i.e., a compound statement containing local declarations).  The
``a_block`` entry pointed to from the statement indicates whether or not the
block has an associated scope.  In C++, a block statement is always generated
for the dependent statement of conditionals (e.g., ``if``), even if no ``{ }``
are present.  However, the block will not have an associated scope unless
something is actually declared inside the block.

The ``stmk_try_block`` statement is used to represent a ``try`` block.  It
points to a compound statement block and a linked list of handlers.  The latter
are entries of type ``a_handler``; each points to a (possibly unnamed)
parameter (the local variable to which the thrown object will be copied when
the handler is invoked) and to an ``stmk_block`` statement, the body of the
handler; for a default handler (i.e., the ``catch(...)`` case) the parameter
pointer in the handler entry is NULL.

The ``stmk_decl`` statement is used to represent a declaration statement.  This
could be the declaration of one or more variables and/or functions (using one
of more comma-separated declarators), one or more typedef types, a local class
or enum type, a using-declaration, etc.

The C++/CLI "for each" statement is represented by an ``stmk_for_each``
statement.  Its representation includes an indication of which "pattern" the
loop matches (the CLI collection pattern, the CLI array pattern, the STL
pattern, or the native array pattern), and for each of those patterns
expressions that give the worked-out expansion for the pieces of the loop, so
that a code-generating back end does not need to figure out the expansion.

When support for variable length arrays is enabled, two additional statements
may appear.

* | An ``stmk_set_vla_size`` statement is associated with a VLA declaration and
    indicates exactly where the dimension expression for a VLA should be
    evaluated.
* | An ``stmk_vla_decl`` statement will refer to a variable of variably
    modified type or to a ``tk_typeref`` type to indicate the location of the
    corresponding variable or ``typedef`` declaration.  This is generally
    important because a VLA type has run-time dependencies that need to be
    honored.  Moreover, when the variable referred by an ``stmk_vla_decl``
    statement has a VLA type (not just a type that "contains" a VLA type), the
    ``stmk_vla_decl`` statement indicates the point at which the storage for
    the variable should be allocated.  Note that a single ``stmk_vla_decl``
    statement may be preceded by multiple ``stmk_set_vla_size`` statements if
    the type associated with the ``stmk_vla_decl`` statement has multiple
    variable-length dimensions.  (See also :ref:`il-types` and
    :ref:`il-operation-exprs`.) In C modes, expression statements with a single
    node of kind ``enk_vla_dealloc`` (see also :ref:`il-vla-dealloc`) may be
    generated to indicate the point at which a variable of VLA type should be
    deallocated (e.g., preceding a ``goto`` out of a scope containing a VLA
    variable).  Such deallocation constructs are generated only when
    ``VLA_DEALLOCATIONS_IN_IL`` is TRUE.

Note that variable-length array types can also appear in ``sizeof``
expressions.  None of the above statement kinds are generated for those cases
since statements cannot in general appear within expressions.  Instead, such
expressions result in ``enk_sizeof`` expression nodes.  Back ends can find and
examine the ``a_vla_dimension`` entries associated with such nodes to evaluate
the result of the ``sizeof`` expression.

.. _src-seq-lists:

Source Sequence Lists
=====================

When ``GENERATE_SOURCE_SEQUENCE_LISTS`` is TRUE, lists of
``a_source_sequence_entry`` are generated, ordered strictly according to
the order in the source program.  This is useful for applications that care
about the exact order of declarations, redeclarations, and definitions in
the source code, e.g., source analysis applications.  Each list entry
represents a declaration, a statement, a macro, a pragma, a template, or a
template specialization that represents an instantiation (if either of the
deprecated configuration macros
``CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS`` or
``NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS`` is TRUE).
Those entities representing declarations and statements point to the
corresponding IL entry, and the latter have pointers back (fields
``source_sequence_entry`` in ``a_source_correspondence`` and in
``a_statement``).

The lists appear in the IL via the ``source_sequence_list`` pointer in
``a_scope`` entries for the file scope and for each function scope.  (The
pointer will always be NULL for other scope kinds.) Source sequence entries
themselves have both forward and backward pointers to facilitate list traversal
and management.

Logically, there is a single source-sequence list in a translation unit.
Actually, however, a number of lists may be involved.  The file-scope
source-sequence list is pointed to from the scope entry for the file scope, but
when a function definition appears, another list, pointed to from the scope
entry for the function, is produced.  The logical successor of the
source-sequence entry for the function (which is in the file-scope list) is the
first entry on the function-scope list, and the logical successor of the last
entry on the function-scope list is the entry pointed to by the ``next``
pointer of the function's source-sequence entry.

The reason for representing the logically continuous source-sequence list by
many discontinuous lists is that source-sequence entries appear in different
memory regions, just as the IL entries they point to do.  Thus, the entries on
the file-scope source-sequence list are allocated in the file-scope memory
region, but the entries on a function-scope source-sequence list are usually
allocated in the memory region of that particular function.

Some declarations within a function, however, create entities that are
allocated at file scope.  This is the case for types, static variables, and
variables and functions declared ``extern``.  The source-sequence entries for
such entries must also be in the file-scope memory region, but that means they
cannot be on the same linked list with entries allocated in the function-scope
memory region.  Therefore, additional side branches are used: entries of type
``a_src_seq_sublist`` ("sublist headers") establish such branches by pointing
to the head and tail of a list of file-scope source-sequence entries that are
logically within the source-sequence list of the function scope.

The sublist headers for a given function, as well as being pointed to by
entries ("sublist parents") in the function-scope source-sequence list, are
also chained together; the head of the linked list is pointed to by
``src_seq_sublist_list`` in the function's scope entry, and its tail is pointed
to by ``last_src_seq_sublist`` in the corresponding scope stack entry.

Entries of type ``a_src_seq_end_of_construct`` are inserted into the
source-sequence list to mark the point at which certain constructs terminate
(the entries point back to the associated IL entry).  This includes class and
enum definitions and statement blocks.  Such entries are also used to mark the
end of a set of source sequence entries for constructs embedded in a variable
or function declaration.  For example, in:

.. code:: c++

   int x[sizeof(struct S { int i; })];

(which is accepted in some modes only), the source sequence list contains an
entry for the definition of variable ``x``, followed by entries for the
definition of struct ``S``, followed by an entry marking the end-of-construct
of variable ``x``\ 's definition.  An end-of-construct marker is similarly used
to mark the appearance of the "``while (...)``" part of a do-while statement
(this is, e.g., significant to be able to record whether a pragma appeared
before the statement as a whole, or just before the ``while`` keyword).

When a declaration is not a definition (e.g., a function declaration with no
body or a variable declared ``extern``), it is represented by an entity of type
``a_src_seq_secondary_decl``, which is pointed to by the source-sequence entry
and points in turn to the corresponding IL entry.  Therefore, when an entity
has more than one declaration, multiple source-sequence entries are created,
but only the one that represents the definition points directly to the IL
entry; the others point to secondary declaration entries.  If there is no
definition of the entity in the compilation, all of the source sequence entries
for that entity will be secondary declarations.

(Source sequence lists are not actually added to the IL tree until
``pop_scope`` is called for a function scope or the file scope.  During front
end processing, the source sequence lists as described above are assembled from
smaller lists that are managed on a per-scope basis; whenever a scope stack
entry is popped, its list is typically merged into the list of the immediately
enclosing scope.  Moreover, the source sequence list describing a function
definition may temporarily be a simple linked list containing a mix of entries
from the memory regions of the function scope and the file scope; the
representation involving sublist headers, as described above, is produced by a
fixup pass when the function scope is popped.)

Many implementations will not need the kind of information provided by
source-sequence lists and will choose to avoid the processing overhead and
especially the memory overhead they introduce.  By default, therefore,
``GENERATE_SOURCE_SEQUENCE_LISTS`` (defined in ``host_envir.h``) is set to
FALSE.

Expressions
===========

``an_expr_node`` describes an expression node, which can be an operator node, a
reference to a constant, variable, or function, an initialization of a
temporary, or a few other things like ``new`` or ``delete``.

Each expression node has a type, indicated by its ``type`` field.

The simplest expression nodes are leaf nodes.  The most common leaf node kind
are:

* | ``enk_variable`` refers to a variable.
* | ``enk_routine`` refers to a function.
* | ``enk_constant`` refers to a constant.
* | ``enk_field`` refers to a field.

Lvalues and Rvalues (Value Categories)
--------------------------------------

Each expression node has an ``is_lvalue`` flag, which is TRUE if the node
represents an lvalue and FALSE if it represents an rvalue.  (It is also TRUE
for what is called a function designator in C, which is simply called an lvalue
in C++.)

So, an ``enk_variable`` node with ``is_lvalue`` TRUE indicates an lvalue for
the variable, and the same node with ``is_lvalue`` FALSE indicates an rvalue
for the variable, i.e., the value of the variable.  In the statement

.. code:: c++

   i = j;

the left operand would have ``is_lvalue`` TRUE and the right operand would have
``is_lvalue`` FALSE.

Similarly, an ``enk_routine`` with ``is_lvalue`` is TRUE indicates an lvalue
for the function (which has a function type), and one with ``is_lvalue`` FALSE
indicates an rvalue for the function (which is the address of the function, and
has pointer-to-function type).

``enk_constant`` nodes are usually rvalues, representing the value of an
indicated constant.  The one exception is that an ``enk_constant`` for a
string constant (``ck_string``) can be an lvalue for the string.

C++11 adds the concept of "xvalues," which are eXpiring values produced by
certain rvalue reference operations.  They are indicated by the ``is_xvalue``
flag in the node being TRUE (``is_lvalue`` and ``is_xvalue`` are never both
TRUE).  Lvalues and xvalues are collectively called "glvalues." The things that
were called "rvalues" in pre-C++11 C++ and in C are known in C++11 as
"prvalues," and the term "rvalue" becomes a collective term for prvalues plus
xvalues.  [#f8]_ In the IL, xvalues in general can be viewed as funny versions
of lvalues.

Glvalue nodes shouldn't be considered to have values.  They're just a
description of an object in memory.  Because they don't have values, they also
can't be considered to be constant or to have constant values, though some
lvalues will have constant addresses (see ``constant_glvalue_address``).

When a glvalue with a cv-qualified type is converted to a prvalue, the type
qualifiers are dropped (except for class-typed prvalues in C++).  In the IL,
this is reflected in the fact that the expression node for the prvalue will
have the unqualified type.  When an lvalue with a function type is converted to
a prvalue, its type becomes pointer-to-function.

Clearing the ``is_lvalue`` flag in an expression node where initially the flag
would be expected to be TRUE indicates a fetch from memory.  This is obvious on
the ``enk_variable`` node, but it also applies to many other nodes, such as
field selection and subscripting operators.  Nodes that can be made to include
a fetch merely by clearing the ``is_lvalue`` flag (or, similarly, the
``is_xvalue`` flag) are referred to as "rvalueable".  One can test for the
presence of an implicit fetch (also known as an lvalue-to-rvalue conversion) in
an expression node by calling ``node_includes_glvalue_to_prvalue_conv``.

.. _il-operation-exprs:

Operation Expressions
---------------------

Expression nodes of kind ``enk_operation`` are operations.  They are the
non-leaf nodes of expression trees, the ones that apply an operator to a set of
operands, to do operations like ``i+j``.

The operands of an operation are linked together as a list, and the operation
node points to the first expression on the list.

Most operations take a fixed number of operands.  The various forms of calls
are exceptions and take a variable number of operands.

The operator kind (addition, assignment, call, etc.) is described by the
``variant.operation.kind`` field, which is mostly independent of the type of
the operands.  For example, an addition is described with kind ``eok_add``
independently of the operand type.  There are a few exceptions to this when the
semantics of the operation vary sufficiently from the generic case to warrant a
separate operator.  For example, pointer arithmetic uses dedicated operators
``eok_padd``, ``eok_psubtract``, and ``eok_pdiff`` instead of the generic
``eok_add`` and ``eok_subtract``.  Where applicable, an additional field
``variant.operation.type_kind`` describes the class of types an operation acts
on.  For example, this will be ``tk_float`` for the multiplication of two
floating-point values, or ``tk_template_param`` for a multiplication involving
a template-dependent operand.  For operations that don't apply to a particular
type kind (e.g., ``eok_call``), the ``type_kind`` field is set to
``tk_unknown``.  The ``type_kind`` field is set automatically when calling
``make_operator_node`` or ``set_node_operator`` (it is ultimately computed by
``operation_type_kind``).

In the case of scalar controlling expressions, the several different cases
(integer, float, and pointer) are reduced to a single case (integer) by
standardizing the expression.  Specifically, for the expression of a
``stmk_if``, ``stmk_while``, ``stmk_end_test_while``, or ``stmk_for``, and for
the controlling operands of the ``eok_not``, ``eok_land``, ``eok_lor``, and
``eok_question`` operators, the expression will always be either an integer
constant ``0`` or ``1``, or an expression whose top operator is defined to
yield a ``0`` or ``1`` result.  Where necessary, a constant value will be
standardized (e.g., ``5.3`` will be changed to ``1``), or a ``!= 0`` operation
of the proper type (integer, float, or pointer) will be added to the top of the
expression.

The operands of an expression may be evaluated in any order consistent with
the C and C++ "as-if" rule.  By default, parentheses are not directly
represented in the IL; their effect is simply reflected in the operands of
the operators in the expression tree or, in cases where they have semantic
implications, by flags in various IL entries (e.g.,
``an_expr_node::is_parenthesized``). Applications that need to be aware of
source-level parentheses can set the ``PARENS_IN_IL`` configuration option;
see :ref:`parentheses`.

In general, operator nodes that do not correspond directly to something that
appeared in the source, and were added by the front end, are marked by having
``compiler_generated`` set to true.

Addressing operators
--------------------

There are several addressing operators (note that when we refer to ``eok_...``
names we mean those under the ``enk_operation`` expression variant):

* | ``eok_address_of`` is the "``&``" operator.  Its operand is an lvalue, and
    its result is a prvalue of pointer type that is the address of the lvalue.
* | ``eok_indirect`` is the "``*``" operator.  Its operand is a prvalue
    pointer, and its result is an lvalue that is the object pointed to.  It
    is rvalueable.
* | ``eok_array_to_pointer`` converts its operand, an array (lvalue, xvalue, or
    prvalue), to a prvalue pointer to the first element of the array.

The "restrict" qualifier on pointers, originally introduced in C99, is a type
qualifier on pointer types that indicates that they can be considered to be
unaliased (nothing else in scope points at what they point at).  The function
``node_is_pointer_with_restrict_semantics`` can be used to test an expression
node that is a pointer (e.g., the operand of an ``eok_indirect`` operation) to
see whether it has the "restrict" semantics.

References
----------

References are not pointers, but they are treated as somewhat pointer-like in
the IL, with their own versions of the addressing operators:

* | ``eok_reference_to`` is the reference version of "``&``".  Its operand is a
    glvalue, and its result is a prvalue of reference type that is the
    "address" of (reference to) the glvalue.  Its operand can also be a class
    prvalue, in which case its result is the "address" of the underlying class
    object.
* | ``eok_ref_indirect`` is the reference version of "``*``".  Its operand is a
    prvalue with reference type, and its result is an lvalue that is the object
    referenced.  It is rvalueable.

These operators do not, of course, appear in the source code; they are added by
the front end where needed.  No operations are allowed on references other than
the above, or storing a reference somewhere, or passing it as an argument.

An example: for something like

.. code:: c++

   int i;
   int &r = i;
   r = r;

the left operand of the assignment is an ``eok_ref_indirect`` with
``is_lvalue`` TRUE over an ``enk_variable`` for ``r`` with ``is_lvalue`` FALSE,
and the right operand is an ``eok_ref_indirect`` with ``is_lvalue`` FALSE over
an ``enk_variable`` for ``r`` with ``is_lvalue`` FALSE.

Note that reference type values are typically dealt with as prvalues, in the
same way that pointers are.  In the example above, the ``enk_variable`` for
``r`` is effectively fetching the pointer-like value stored in the reference
variable, so its ``is_lvalue`` flag is FALSE.  An ``enk_variable`` node with
``is_lvalue`` TRUE for a reference variable would mean an lvalue for the cell
containing the reference value, but there's no real use for that in the C++
language, since you can't take the address of the cell nor can you change its
value after initialization.

Address constants with kind ``ck_address`` can have reference type when they
are used to initialize a reference-typed variable.  They represent a constant
reference value.

Rvalue references (source form like "``int``\ ``&&r``") are similar to the
traditional "lvalue" references.  ``eok_reference_to`` and ``eok_ref_indirect``
can be used with them also, and ``ck_address`` constants can have rvalue
reference type.  An rvalue reference is always bound to an rvalue (i.e., an
xvalue or prvalue).  See :ref:`casts` for information on casts to
rvalue reference types.

Field Selection
---------------

There are several operators that implement field selection:

* | ``eok_dot_field`` is the "``.``" operator.  The left operand is a class
    glvalue or prvalue, the right operand is an ``enk_field``, and the result
    is a glvalue or prvalue of the type of the selected field (with
    cv-qualifier adjustment).  When the left operand is an lvalue, the result
    is an lvalue (rvalueable), and similarly for an xvalue.  When the left
    operand is a prvalue, the result is a prvalue.
* | ``eok_points_to_field`` is the "``->``" operator.  The left operand is a
    prvalue pointer to class, the right operand is an ``enk_field``, and the
    result is an lvalue (rvalueable) of the type of the selected field (with
    cv-qualifier adjustment).
* | ``eok_pm_field`` is the "``.*``" operator.  The left operand is a class
    glvalue or prvalue, the right operand is a pointer to data member, and the
    result is a glvalue or prvalue of the type of the pointed-to member (with
    cv-qualifier adjustment).  When the left operand is an lvalue, the result
    is an lvalue (rvalueable), and similarly for an xvalue.  When the left
    operand is a prvalue, the result is a prvalue.
* | ``eok_pm_points_to_field`` is the "``->*``" operator.  The left operand is
    a prvalue pointer to class, the right operand is a pointer to data member,
    and the result is an lvalue (rvalueable) of the type of the pointed-to
    member (with cv-qualifier adjustment).
* | ``eok_dot_static`` is the "``.``" operator when the entity selected is
    static.  The first operand is a class glvalue or prvalue (and it *is*
    evaluated) and the second operand is an ``enk_variable`` for a static data
    member, an ``enk_routine`` for a static member function, or an
    ``enk_constant`` identifying a member constant (e.g., an enumerator).  The
    result is the second operand.
* | ``eok_points_to_static`` is the "``->``" operator when the entity selected
    is static.  The first operand is a prvalue pointer to class (and it *is*
    evaluated) and the second operand is an ``enk_variable`` for a static data
    member, an ``enk_routine`` for a static member function, or an
    ``enk_constant`` identifying a member constant (e.g., an enumerator).  The
    result is the second operand.

Note that most of the selection operators are rvalueable in some cases, but the
static variants are not; for those, the lvalue-to-rvalue conversion, if there
is one, has to be indicated on the second operand, and not in the
``eok_dot_static`` or ``eok_points_to_static`` operation itself.

Also note that there are no variants of the pointer-to-member selections for
pointer-to-member-function cases.  Those can be used only in calls, and the two
operands of the selection are simply folded into the ``eok_pm_call``.
Likewise, the "``.``" or "``->``" operation in a member function call is simply
folded into the call.

Subscripting
------------

``eok_subscript`` is the array subscripting "``[]``" operator.  One operand is
a prvalue pointer to the first element of the array, and the other is an
integral subscript value.  The result is an lvalue (rvalueable) for the
specified element of the array.  Usually, the pointer operand is first, but
that's not actually a requirement of the language, and the operands can appear
in either order, as for the expression "``1[a]``".  See the
``pointer_operand_is_second`` field, which indicates this unusual case, and the
macro ``subscript_or_padd_pointer_operand``, which can be used to extract the
pointer operand.  As you will note from the name of that macro, this is also an
issue for the pointer addition (``eok_padd``) operator.

.. _casts:

Casts
-----

The ``eok_cast`` operator is used to indicate simple casts.

Casting a derived class pointer to a base class pointer is represented by an
``eok_base_class_cast`` operator.  Casting a base class pointer to a derived
class pointer uses an ``eok_derived_class_cast`` operator.  These casts differ
from ordinary pointer casts in that they may add or subtract an offset to the
pointer, and they must preserve a NULL pointer.  They can also be applied
directly to a class glvalue or prvalue, rather than a pointer to a class
object, or to a C++/CLI handle.

``eok_pm_base_class_cast`` and ``eok_pm_derived_class_cast`` are the similar
casts for pointers-to-members.  The same operators are used for
pointers-to-data-members and pointers-to-member-functions, even though the
implementations of those are likely to be different.

The anachronism of casting a bound pointer-to-member-function to a normal
pointer (which is similar to the processing done by an
``eok_points_to_pm_call`` to determine the function to call) is rendered as an
``eok_virtual_function_ptr`` operation.

In ``pcc`` mode and in some Microsoft and GNU C++ modes, certain casts to
like-sized types (like ``int`` to ``unsigned int``) leave a result that is
still an lvalue.  For these cases, the lvalue is cast to the appropriate type
using the ``eok_lvalue_cast`` operator.  The result is still an lvalue.

The C++ ``dynamic_cast`` operator is rendered as an ``eok_dynamic_cast``
(pointer or handle case) or ``eok_ref_dynamic_cast`` (reference case), but only
for cases that require runtime processing.  The rest are rendered as ordinary
casts.

Casts to ``bool`` are rendered as an ``eok_bool_cast``, which has the same
effect as a "``!= 0``" test.

The ``eok_lvalue_adjust`` operator adjusts the type of a glvalue.  Its operand
is a glvalue, and its result is a glvalue for the same object but with a
different type.

The ``eok_class_rvalue_adjust`` operator adjusts the type of a class prvalue.
It is limited to adjusting the cv-qualifiers of the type.  Its operand is a
class prvalue, and its result is the same value with a different type.  When
``LOWER_CLASS_RVALUE_ADJUST`` is TRUE, IL lowering will rewrite these
adjustments by taking the address of the class rvalue, casting the pointer to
the proper type, and indirecting to get back to a class object.

Casts to reference types are represented as glvalue-to-glvalue operations.  The
simplest cases are represented by the ``eok_ref_cast`` operator.  The node type
is the underlying type of the reference, e.g., the node for a cast ``(T &)x``
has type ``T``, not type ``T&``.  The operand is a glvalue, and the result is a
glvalue for the same object but with the type ``T``.  More complicated
reference casts that involve related-class adjustments are represented by
``eok_base_class_cast`` and ``eok_derived_class_cast`` operators operating on
class glvalues.  Nodes that represent a cast to a reference type are marked
with the ``is_reference_cast`` flag.  If several nodes are required (e.g.,
multiple casts to base classes) only the topmost node will be marked.  Note
that ``eok_ref_cast`` nodes are rvalueable, and when the ``is_lvalue`` flag is
cleared to indicate the presence of an lvalue-to-rvalue conversion the node
type becomes the cv-unqualified type (for non-class lvalues) or pointer-to the
lvalue type (for function lvalues).  That means, for a function type ``F``, a
cast to ``F&`` with an included lvalue-to-rvalue conversion is represented by a
node with type ``F*``.

Casts to rvalue reference types are similarly represented by ``eok_ref_cast``,
``eok_base_class_cast``, and ``eok_derived_class_cast`` nodes.  They are marked
with the ``is_rvalue_reference_cast`` flag in addition to the
``is_reference_cast`` flag.  Although in the C++ language such reference casts
take an rvalue as their operand, in the IL the operand is always converted to a
glvalue and then cast.  The node or nodes that do the cast are then exactly the
same as those that would be used for an lvalue reference cast.  The final
(topmost) cast node will always be an ``eok_ref_cast`` node, and it will have
``is_lvalue`` FALSE to indicate the result is an rvalue (unless the result
needs to be converted back to an lvalue for some reason).

Assignments
-----------

There are assignment operators for integral types, fixed-point types, floating
types, classes (bitwise copy, or C ``struct`` assignment), pointers, and
pointers-to-members.

There is also a block-copy assignment, ``eok_bassign``, which is only generated
by IL lowering and inside front-end-generated assignment operator functions,
and is needed to do bitwise copies of arrays when copying classes.  It is
unusual in that its second operand is in lvalue form and its result type is
``void``.

In C++, assignment operators can return lvalues instead of rvalues.  Such cases
are indicated by the ``returns_lvalue_instead_of_usual_rvalue`` flag in the
expression node.  The expression type matches the type of thing returned (i.e.,
it matches the first operand's type).  The same flag is used for prefix ``++``
and ``--`` and the "``?``" and "``,``" operators when they return an lvalue.
When that flag is set, ``is_lvalue`` will also always be set as well (in other
words, they are not rvalueable).

Compound Assignments
--------------------

In cases where a compound assignment operator (e.g., ``+=``) is applied to
operands of two different types, the right side operator will be cast as
necessary to the type of the operation (as it would be for other operators).
The left side operand (the destination, an lvalue), however, is left in its
original type, which is also the type of the entire expression.  The back end
must provide a cast of the value of the left operand to the operation type
(generally the type of the right operand, but see below) before performing the
operation on the operands.  For example: in ``i-=f`` (``i`` an ``int``, ``f`` a
``float``), the back end must provide the cast of ``i`` to ``float`` before
doing the subtraction; the operation type is ``float``; the result type (and
the type of the expression node) is ``int``.

As mentioned above, the operation type for the operation is generally the right
operand type.  There are two exceptions in which the underlying type of the
left operand is the operation type: shift assignments (``<<=`` and ``>>=``) and
pointer ``+=`` and ``-=`` (for those cases, the right operand is not cast to
the operation type).  Fixed-point ``+=``, ``-=``, ``*=``, and ``/=`` are yet
more complicated; the function ``compound_assignment_operation_type`` should be
called to determine the operation type for those (it also works for all the
simpler cases).

GNU vector cases are even weirder, as no attempt is made to bring the operands
to a common type.

Binary Conditional Operator
---------------------------

In some configurations, the front end supports a GNU extension known as the
"binary conditional operator".  The operator has the form ``expr1``\ ``?``\
``:``\ ``expr2`` (i.e., similar to the regular conditional operator, but with
an omitted middle operand).  Such expressions result in the value of ``expr1``
if ``expr1`` converted to a boolean is true, or the value of ``expr2``
otherwise.  In the IL, this is represented by the usual ``eok_question``
operator, but with the flag ``is_gnu_two_operand_question_mark`` set to TRUE.
The expression includes a synthesized second operand, which may use an
``enk_reuse_value`` expression node to reuse the value of the first operand
without evaluating it a second time.

Calls
-----

The normal call is rendered as an ``eok_call`` operation.  The first operand
gives the address of the routine to call, and the rest of the operands are the
arguments for the call.

For nonstatic member function calls, the operation is ``eok_dot_member_call``
(for calls in the ``x.f()`` form) or ``eok_points_to_member_call`` (for calls
in the ``p->f()`` form).  The first operand is the routine address (as a normal
pointer, not a pointer-to-member); the second operand is the selector object (a
class glvalue or prvalue for ``eok_dot_member_call``, or a pointer to a class
object for ``eok_points_to_member_call``).  The remaining operands are the
arguments to the call.  In either case, a call with virtual semantics is
indicated by the ``is_virtual_call`` flag in the expression node.

For pointer-to-member-function calls, the operation is ``eok_dot_pm_call`` (for
calls in the ``(x.*pmf)()`` form) or ``eok_points_to_pm_call`` (for calls in
the ``(p->*pmf)()`` form).  The first operand is the pointer-to-member-function
(i.e., a value of pointer-to-member-function type, not a normal pointer type);
the second is the selector object (a class glvalue or prvalue for
``eok_dot_pm_call``, or a pointer to class for ``eok_points_to_pm_call``); the
rest are the actual arguments, as above.

If a routine has default arguments, the front end will supply the default
expressions if no explicit arguments are provided.  The back end need not do
any special handling.

If a parameter of class type must be passed by using a copy constructor, a call
of the copy constructor into a temporary will be generated on the calling side.
Somewhat arbitrarily, the ``enk_temp_init`` node for that is marked as an
lvalue.

For routines that return a class object via a copy constructor, the call will
always be immediately underneath a ``dik_call_returning_class_via_cctor``
dynamic initialization entry.  The entity being initialized by the dynamic
initialization entry should be taken as the destination for the copy
constructor call.  (IL lowering makes this explicit.)

Note that uses of operator functions and conversion functions are rendered as
routine calls even through they use operator syntax or are implicit in the
source program.  Constructor and destructor "calls" generally only show up in
dynamic initialization entries, and not as standard calls in expressions.

Inlining is considered to be a back end issue.  The front end does not do
inlining of calls.  IL lowering optionally offers a limited form of inlining,
but it's intended only for those who have no better alternative.

Returns
-------

The ``stmk_return`` statement is used to represent all returns, whether
explicitly written by the programmer or generated by the front end.  In
addition to the expected

.. code:: c++

   return;
   return expr;

(which are distinguished by a NULL or non-NULL ``expr`` pointer), there is also
a special form of return used for returns in routines that return a class value
by calling a copy constructor.  For those, the ``stmk_return`` statement has a
pointer to a dynamic initialization entry, which indicates the initializing
operation to be done to return the class object.  IL lowering implements this
by having the caller pass an extra argument that provides the address at which
the class object should be placed, and having the called routine call the copy
constructor using the implicit parameter as its destination address.

Pointers to Members
-------------------

Pointer-to-member types are represented as an abstraction in the unlowered C++
intermediate language.  It is up to IL lowering or a back end to translate them
into a particular implementation.  Typically, pointers to data members are
implemented as a small integral offset (the offset of the member within the
class) and pointers to member functions are rendered as a structure (see the
ARM, 8.1.2.c).

Pointer-to-member types are represented by a ``tk_ptr_to_member`` type entry.

Pointer-to-member constants are represented by a constant entry of kind
``ck_ptr_to_member``.  These represent source constructs like ``&A::x``,
meaning some sort of representation of the location of ``x`` in a class object
of type ``A`` (not a pointer to that member in a specific object, but rather a
representation of how to find that member in an arbitrary entity of that
class).

Operands of pointer-to-member type can be assigned (``eok_assign``) and
compared (``eok_eq`` and ``eok_ne``).  They can also be cast to base or derived
classes (``eok_pm_base_class_cast`` and ``eok_pm_derived_class_cast``).

Temporary Initialization
------------------------

The ``enk_temp_init`` node is used to introduce a temporary in the middle of an
expression.  It points to a dynamic initialization entry and an expression.
The initialization indicated in the dynamic initialization entry is performed
on the temporary, and then the temporary is returned as the value of the
``enk_temp_init`` node, as an lvalue, xvalue, or prvalue.  Any destruction
indicated in the dynamic initialization entry must be done to the temporary at
the end of the object lifetime to which the dynamic initialization is bound.

Condition Declarations
----------------------

In ``if``, ``while``, ``for``, and ``switch`` statements in C++, the expression
tested can be a condition declaration.  For example:

.. code:: c++

   if (float x = f()) { ... }

This is represented in the IL as an expression node of kind ``enk_condition``.
The condition has its own associated scope, which contains just the one
variable, and which wraps around the statement.  Note that the initializations
for conditions in loops are done each time around the loop; if destruction is
also required, it too must be done each time around the loop.

New and Delete
--------------

A ``new`` operator allocates storage and optionally initializes it.  It is
represented by an ``enk_new_delete`` expression node.  The supplement to that
node provides information about the ``new`` routine to be called, the arguments
for that call, and optionally a dynamic initialization entry that describes any
initialization to be done.  If the allocation fails (i.e., the ``new`` routine
returns NULL), the initialization is not done.

A ``delete`` operator deallocates storage after optionally calling a
destructor.  It is represented by an ``enk_new_delete`` expression node.  The
supplement to that node provides information about the ``delete`` routine to be
called, the pointer to the object to be deleted, and optionally a dynamic
initialization entry that describes destruction to be done.

In both the ``new`` and the ``delete`` cases, the routine indicated in the
new/delete supplement may be NULL to indicate that the allocation or
deallocation should be done by the constructor or destructor.  The dynamic
initialization entry in those cases indicates the appropriate constructor
or destructor.  This alternative is controlled by configuration options
``NEW_CAN_BE_FOLDED_INTO_CTOR`` and ``DELETE_CAN_BE_FOLDED_INTO_DTOR``,
which must be true if assignment to "``this``" (an anachronism) is
supported, because the constructor or destructor must have control of
allocation and deallocation in that case.

When a ``delete`` contains the count of array elements (an anachronism), the
expression between ``[]`` is scanned and thrown away; it does not appear in the
IL.

The C++/CLI ``gcnew`` operator is represented by an ``enk_gcnew`` expression
node.  For CLI array cases, the supplment provides both the bound sizes
(whether explicitly specified or determined from the size of the initializer)
and an initializer aggregate for the element values.

Lambda Expression
-----------------

A lambda expression -- which produces a temporary "closure object" of a
class type with an ``operator()`` member -- is represented by an
``enk_lambda`` expression node.  Such a node points to a supplement entry
of type ``a_lambda`` and to an ``a_dynamic_init`` entry describing the
initialization and (if applicable) the destruction of the temporary
"closure object" resulting from the expression.  The ``a_lambda`` entry
describes the static properties of the lambda expression, such as the local
variables it captures (and how they are captured), the closure type,
relevant source positions, etc.

Throw Expression
----------------

A ``throw`` expression is represented by an ``enk_throw`` expression node.  It
points to a supplement that points to a dynamic initialization entry that
describes the object to be thrown (or rather, how to make a copy of it, since a
copy is what's actually thrown); the supplement pointer is NULL in the case of
a "rethrow".

Sizeof
------

When a ``sizeof`` operator cannot be evaluated at compile time (for instance,
when it is applied to a variable length array), an ``enk_sizeof`` expression
node is put out.  Such nodes are also used to represent dependent ``sizeof``
expressions in prototype instantiations.

typeid
------

The ``typeid`` operator is represented by an ``enk_typeid`` expression node.
It points to the type whose type information is wanted.  For cases that require
runtime type determination (e.g., ``typeid(*p)`` where ``p`` points to a
polymorphic class type), there is also a pointer to an expression whose dynamic
type is to be determined at runtime.

``enk_typeid`` is also used for the C++/CLI *X*\ ``::typeid`` feature.  That's
indicated by the ``is_cli_typeid`` flag.  The result value in that case is a
handle to a ``System::Type`` entry.

.. _il-vla-dealloc:

VLA Deallocation
----------------

In C mode, the front end may generate ``enk_vla_dealloc`` nodes to indicate
where VLAs should be deallocated.  Such nodes may also be created when lowering
C++ IL for VLAs (when ``DO_IL_LOWERING`` is TRUE).  The nodes are eliminated
from the IL altogether (both in C and C++ modes) if
``LOWER_VARIABLE_LENGTH_ARRAYS`` is TRUE; the nodes are translated to calls
into the run-time support library.

Parameters
----------

When an identifier in an expression refers to a parameter, the resulting IL is
an ``enk_variable`` node if the parameter is referenced in the body of a
function.  However, it is possible to refer to parameters in expressions that
appear *outside* the function body.  For example, in C++11, a function template
declaration might be as follows:

.. code:: c++

   template<class T> auto f(T p)->decltype(*p);

Such uses always appear in unevaluated contexts and are represented with
``enk_param_ref`` nodes, which, like ``enk_variable`` nodes, are rvalueable.

An ``enk_param_ref`` node doesn't point directly to IL representing a
parameter (e.g., ``a_param_type`` entries) because such a pointer might
become invalid when, e.g., two function types are combined through
``composite_routine_type``.  Instead, the parameter is represented by two
integers: The parameter position in its enclosing parameter list, and the
"level" of the parameter list relative to the point of reference (function
declarators may be nested and an inner declarator may refer to a parameter
of an enclosing declarator).

Fixed-point operations
----------------------

Operations on fixed-point values are represented using the usual expression
operators (``eok_negate``, ``eok_post_incr``, ``eok_add``, ``eok_divide``,
``eok_eq``, ``eok_assign``, etc.).  For mixed-type arithmetic (i.e., operations
involving two different fixed-point types, or a fixed-point type and an integer
type), these operators are different from most of the other (more common)
operators in that their operands are not converted to a common type.  For
example, when adding a ``short int`` to a ``long _Accum``, the operator is
applied directly to the unconverted operands without "usual arithmetic
conversions." A back end is then responsible for performing the operation with
maximum precision and converting the result to the result type indicated in the
operator expression node.  This arrangement is necessary to meet the
requirements of the Embedded C extensions (ISO/IEC TR 18037).

Note that while mixed-type fixed-point arithmetic does not in general attempt
to convert the operands to a common type, a conversion from unsigned to signed
may be applied if one operand is signed and the other not.  Furthermore, an
operation involving a fixed-point operand and a floating-point operand is a
floating-point operation: The fixed-point operand is converted to a
floating-point type.

.. _parentheses:

Parentheses
-----------

Parentheses are represented in the IL as expression operation nodes with an
operator of ``eok_parens``.  Those are not usually generated; ``PARENS_IN_IL``
must be set to TRUE to request them.  When that flag is TRUE, each pair of
parentheses in a source program expression is represented by an ``eok_parens``
node in the IL for the expression.  If ``EXTRA_SOURCE_POSITIONS_IN_IL`` is also
TRUE, those expression nodes will include position information on the opening
and closing parenthesis tokens.

``eok_parens`` nodes are not used or allowed in versions that use IL lowering.
They are intended mostly for source-analysis applications.  Note that enabling
``PARENS_IN_IL`` should not be done without reason, because it complicates the
code that deals with expressions.  Every bit of code that wants to check for a
specific operator in an expression may have to check for and strip parentheses
off the expression before doing the check.  That can be done by calling
``skip_parens``, but it's hard to always remember to do that.  (This problem is
very similar to the ``skip_typerefs`` problem for types, and that is widely
known as a pain and a source of bugs.)

Fold Expressions
----------------

A C++17 fold-expression like "``(ps + ...  + 1)``" where ``ps`` contains an
unexpanded parameter pack is represented in generic template contexts (i.e.,
"prototype instantiations") using an ``enk_fold`` node.  The expansion of the
construct (in real template instantiations), however, uses the ordinary binary
operator representations.  For example, if ``ps`` expands to a sequence of
integer parameter variables, the expanded representation will involve a tree of
``enk_operation``/``eok_add`` nodes.

Boxing and Unboxing
-------------------

C++/CLI boxing and unboxing is implemented by several operators:

* | ``eok_box`` takes an operand of a value type (e.g., ``int``) and returns a
    handle to a boxed version of the value on the CLR heap (e.g., handle to
    ``System::Int32``).
* | ``eok_handle_to_box`` has the same runtime meaning as ``eok_box``, but is
    generated for source uses of the unary ``%`` operator applied to a value
    class.
* | ``eok_unbox`` takes an operand that is a handle to a boxed value class or
    boxed enum and returns an lvalue (in place on the heap) or rvalue (copied)
    for the contained value, depending on the ``is_lvalue`` flag.
* | ``eok_unbox_lvalue`` is similar, but the operand is an lvalue for the value
    class or boxed enum.  It is always compiler-generated, e.g., on top of an
    ``eok_indirect`` applied to a handle to a value class.

Pragmas
=======

A ``#pragma`` directive may be represented in the IL by an entry of type
``a_pragma``, which appears on a linked list pointed to by the ``pragmas``
field of an IL scope entry.  It includes a pointer (see the ``entity`` field)
if it is specifically associated with a statement, routine, etc.; the entity
does not point back, but rather has its ``has_associated_pragma`` flag set,
since more than one pragma entry may be associated with a given IL entity.  For
additional information, see :ref:`pragmas-and-attributes`.

.. _anonymous-unions:

Anonymous Unions
================

Anonymous unions are unions that have no name, either for the union type or for
the singular instance of the union.  The names of the members of the union are
promoted into the surrounding context.  There are two kinds of anonymous
unions:

* | Anonymous unions within classes, as in

  .. code:: c++

     class A {
       union {
         int i;
         float j;
       };
     } x;

  | Here, the fields of the anonymous union are promoted into the surrounding
    class, so one can refer to ``x.i``.
* | Anonymous unions as variables, as in

  .. code:: c++

     union {
       int i;
       float j;
     };

  | Here, ``i`` can be used as if it were a variable.  Such constructs can
    appear at file and namespace scope and within function and block scopes.

For the first (field) case, the IL reflects the source form: class ``A`` will
contain an unnamed field whose type is an unnamed union, which in turn contains
the fields ``i`` and ``j``.  A reference like ``x.i`` appears as a selection of
``i`` directly from ``x``.  By looking at the field for ``i``, one can tell
that it is a member of an anonymous union, and infer the additional field
selection.  (IL lowering adds the extra field selection.)

For the second (variable) case, there will be an unnamed variable whose type is
the unnamed union.  A reference like ``i`` appears as a selection of ``i`` out
of the unnamed variable, so it is more explicit (and less like the source
program) than the field case.

Virtual Function Tables
=======================

There are no "virtual function tables" *per se* in the unlowered IL.  There is
only the information that is part of the definition of virtual functions in the
language:

* | Functions are marked as being virtual, including when they are implicitly
    virtual because they override a virtual function from a base class.
    Virtual functions are assigned ordinal numbers within a class, which can be
    used as their indexes into a virtual function table.
* | Calls to virtual functions use a special operator, ``eok_virtual_call``.
* | Base class entries contain information on virtual function overrides, i.e.,
    on which virtual functions of a base class are overridden by virtual
    functions in derived classes (either the one the base class entry is
    attached to or some class on the derivation path between the base class and
    that one).

One can generate a virtual function table for ``A`` in ``B`` by scanning the
member functions of ``A`` looking for virtual functions, along with the
override list for base class ``A`` of ``B``.  If a function in ``A`` does not
appear on the override list, the function from ``A`` goes into the virtual
function table.  Otherwise, the function from the override list goes into the
virtual function table.  More detail can be found in the code in IL lowering
that generates virtual function tables.

.. _il-c99-features:

C99 features
============

The C99 IL extensions over C89 are as follows:

* | For variable-length arrays (VLAs), the front end generates two additional
    statement kinds (``stmk_set_vla_size`` and ``stmk_vla_decl``) and
    optionally one additional expression node kind (``enk_vla_dealloc``; see
    :ref:`il-vla-dealloc`).  Types and variables are also marked as being VLAs
    when appropriate.  This feature can be enabled also in C89 and C++ modes
    (by ``--vla``), and is controlled by the macro ``VLA_ALLOWED`` (which is
    set by default if ``C99_IL_EXTENSIONS_SUPPORTED`` is set).  If
    ``LOWER_VARIABLE_LENGTH_ARRAYS`` is TRUE, the VLA-specific constructs are
    eliminated from the IL (this requires the run-time support library).
* | For compound literals, an ``enk_temp_init`` node is generated.  Such a node
    generates a temporary, and is otherwise used only in C++ mode.  This
    feature can be enabled also in C89 mode (by ``--compound_literals``), and
    is controlled by the macro ``COMPOUND_LITERAL_ENABLING_POSSIBLE`` (which is
    set by default if ``C99_IL_EXTENSIONS_SUPPORTED`` is set).  C99 lowering
    eliminates compound literals by rewriting them as initialized generated
    variables.
* | For designators in initializers, ``ck_designator`` constants are
    generated in aggregate initializers.  This feature can be enabled also
    in C89 mode (by ``--designators``), and is controlled by the macro
    ``DESIGNATED_INITIALIZER_ENABLING_POSSIBLE`` (which is set by default
    if ``C99_IL_EXTENSIONS_SUPPORTED`` is set).  If
    ``LOWER_DESIGNATED_INITIALIZERS`` is TRUE (which it is by default if
    C99 lowering is done), designators are almost completely eliminated;
    they remain only for initializations of members other than the first in
    unions, which are not possible in C89.
* | For ``_Bool``, the type is represented as in C++ (the ``bool_type`` flag is
    set in the integral type).  C99 lowering eliminates ``_Bool`` by rewriting
    the type as a typedef to the underlying integer type.
* | For complex and imaginary types, there are new type kinds (``tk_complex``
    and ``tk_imaginary``), new constant kinds (``ck_complex`` and
    ``ck_imaginary``), and some new operator kinds (e.g., ``eok_jfadd`` and
    ``eok_jmultiply``) and conversions.  If ``LOWER_COMPLEX`` is TRUE, C99
    lowering rewrites complex types as structs and imaginary types as
    floating-point types, rewrites the constants as aggregate or floating-point
    constants, and rewrites the operators and conversions as runtime routine
    calls.
* | For flexible array members, the last member of a struct type may have an
    array type with an unknown bound.  No lowering is done.
* | For non-constant expressions in aggregate initializers, the aggregate
    constants contain ``ck_dynamic_init`` constants, as in C++.  C99 lowering
    rewrites those as executable code.
* | For inline functions, the function is marked with the ``is_inline`` and
    ``suppress_inline_body`` flags, as in C++.  If ``MINIMAL_INLINING`` is
    TRUE, C99 lowering replaces inline function calls with the expansions where
    possible (as is done in C++).
* | For ``long``\ ``long``, there are two new integer kinds (``ik_long_long``
    and ``ik_unsigned_long_long``) and new constants of these kinds.  This
    feature can be enabled also in C89 mode and in C++ mode, and is controlled
    by the macro ``LONG_LONG_ALLOWED`` (which is set by default if
    ``C99_IL_EXTENSIONS_SUPPORTED`` is set).  No lowering is done.
* | For mixed statements and declarations, the statements and declarations
    appear intermixed in the statement list of a compound statement, as in C++.
    No lowering is done.

Entry Prefix
============

Each IL entry is preceded by a prefix, which is not part of the IL entry proper
but contains information about the entry that is useful to tree walking
routines and the like:

* | Is the entry allocated in the file-scope memory region?
* | Has the entry been encountered yet on a tree walk of the IL?
* | Has the entry been encountered yet by IL lowering?
* | What entry number has been assigned to the entry (for the alternate IL file
    format)?

This prefix is allocated in space preceding the beginning of the entry (that
is, one has to do special address arithmetic to get to the prefix; see the
macro ``il_entry_prefix_of``).  Some of the flags mentioned above are present
only when required by the front end configuration.

Orphans
=======

Type entries and static variables that are local to a function require some
special handling.  All type entries and all static variables are allocated in
the file scope memory region.  However, types and static variables that are
local to a function are placed on the types and static variables list of a
function or block scope.  Those are unusual lists: the scope entry (in a
function scope memory region) points to a list that is entirely in the file
scope memory region.  Yet, the entries pointed to are not really part of the
file scope.  They might, in fact, not be referenced from anywhere except the
function scope memory region.  Such entries are "orphaned" in the file scope:
their parents can be processed, written out, and removed from memory, but the
children remain in the file scope, unattached to the rest of the file scope IL
tree.

A similar problem occurs when an IL entry such as a type is allocated in the
file scope memory region because it might be shared.  If the entity is
referenced only from function scope memory regions, it too will be an orphan.

Orphans are a problem if one wants to process the intermediate language on a
per-memory-region basis.  One needs to be able to find all of the orphans when
processing the file scope memory region.  There are two mechanisms that help
with this:

* | ``il_header`` points to a list of entries of type
    ``a_scope_orphaned_list_header``.  Each entry on this list holds a copy of
    the local type and static variable list pointers for a function or block
    scope that has such entries.  The ``a_scope_orphaned_list_header`` entries
    are allocated in the file scope memory region, so they can be used to find
    all such lists if the function and block scopes are no longer in memory.
  |
  | See the routine ``add_scope_orphaned_il_lists``.
* | Each file-scope IL entry is preceded by space for a pointer.  (This space
    precedes the entry prefix.) The pointer is used to chain potential orphans
    onto a list of entries of the same kind headed by an element of the global
    array ``orphaned_file_scope_il_entries``.  Those lists can be traversed
    after processing the file scope to find entries that might otherwise be
    lost.  Note that entries are placed on the list if they are potential
    orphans, i.e., if they are allocated in the file scope memory region and
    are referenced from a function scope memory region.  Most of the entries on
    the lists will not be orphans, but all orphans will appear on the list.
  |
  | See the routine ``add_orphaned_file_scope_il_entry``.

All orphan processing is included only if needed.  It is needed if IL lowering
is done or if an IL file is written.

Relationship of the IL and the Symbol Table
===========================================

The symbol table of the front end is not part of the intermediate language
structure and is not passed to the back end.  It is really mostly a name lookup
mechanism, which one can use to look up a name to get to the associated
intermediate language entry.  By the time a back end receives the IL, all
references to entries are simply pointers to the proper entries, and therefore
no name lookup mechanism is needed.

The symbol table also contains some information that is only needed in the
front end (for error checking or searching) and is not passed to the back end,
or information that has no IL representation (for example, symbol entries for
keywords).

The symbol table also always reflects the true C and C++ idea of name scoping,
which may not exactly match the IL representation.  For example, if we have the
declaration ``extern int xx`` in two scopes, each declares a name ``xx`` whose
visibility is limited to the scope in which it is declared, but the object (an
integer variable) associated with the name exists over the entire program.
During the front end compilation process there would be two different symbol
entries for ``xx`` in two different scopes, but in the intermediate language
there would be only one variable entry for ``xx``, and it would appear at the
file scope level.  All references to either ``xx`` of the source program would
point to that one entity, and would not indicate which of the original ``xx``\
s was referenced.

.. _templates-in-the-il:

Templates
=========

The front end generates IL for instances of templates.  For example, class
types are generated for instances of class templates and routines are
generated for instances of function templates.  A back end can view a
template as a sort of powerful macro expanded by the front end, and can
deal with the generated IL for instances exactly as if the instances were
non-template classes and routines.  A code generator need not, in general,
care that the source program contained templates.

Each template declaration or definition in the source program is
represented by an IL entry of type ``a_template``.  A list of those is
attached to the ``templates`` field of ``a_scope``.  A separate
``a_template`` entry is included for each declaration or definition of a
given template (i.e., there can be more that one ``a_template`` entry
associated with a single template; if there is, the first such entry is the
canonical one, and is used when referring to the template itself as opposed
to a given declaration of the template).  When the macro
``ALL_TEMPLATE_INFO_IN_IL`` is set to TRUE, ``a_template`` entries are also
generated for member functions, static data members, and member classes
nested in class templates.  If ``RECORD_TEMPLATE_STRINGS`` is TRUE, the
``a_template`` entry contains a pointer to a null-terminated string that is
a textual representation of the template declaration or definition.

The generated IL for an instance generated from a template does preserve some
indications of the fact that the entity came from a template:

* | The type entry variant for class/struct/union has an ``is_template_class``
    flag that is set for template classes.  If the class is an instance of a
    template, rather than a member of a class that is a template, the class
    type supplement's ``template_arg_list`` field gives the list of template
    arguments for the template.
* | The routine entry has an ``is_template_function`` flag that is set for
    template functions.  If the function is an instance of a template, rather
    than a member of a class that is a template, the routine entry's
    ``template_arg_list`` field gives the list of template arguments for the
    template.
* | The variable entry has an ``is_variable_template`` flag that is set for
    template variables and template static data members.
* | The variable and routine entries have instantiation information flags.

Template argument lists are represented by a list of ``a_template_arg``
entries.  Each entry points to an argument value: an IL type for a type
template argument, an IL constant for a nontype template argument, or an
``a_template`` entry for a template template argument.

More information about templates is available when the macro
``ALL_TEMPLATE_INFO_IN_IL`` is set to TRUE.  With that setting:

* | The ``assoc_template`` field (in a class type supplement, routine, or
    variable) points to the (canonical) ``a_template`` entry for the associated
    template.
* | A ``canonical_template`` field of ``a_template`` points the canonical
    ``a_template`` entry for the associated template.
* | A ``definition_template`` field of ``a_template`` is set in the canonical
    entry to point to the ``a_template`` entry associated with the template
    definition (if any).  In other entries it is NULL.

When ``ALL_TEMPLATE_INFO_IN_IL`` is TRUE and, in addition, the variable
``prototype_instantiations_in_il`` is TRUE, the IL tree includes IL for
templates themselves (as opposed to IL for instances of templates).  This IL is
produced by doing a "prototype instantiation" of the template, which means
doing syntax and (partial) semantic analysis of the template definition without
the use of actual template argument values.  Of necessity, such analysis is
incomplete; the meaning of certain constructs cannot be determined until the
actual template argument values are known.  Therefore, the IL for prototype
instantiations can contain special generic operators and variants of other IL
constructs that do not appear in normal IL (e.g., ``eok_lvalue``).

Prototype instantiations of classes are always done.  Prototype instantiations
of functions are done when the variable ``nonclass_prototype_instantiations``
is TRUE (see the command-line option ``--parse_templates``).  The variable
``prototype_instantiations_in_il`` controls whether prototype instantiations
are *preserved* in the IL.  If the flag is FALSE, the instantiations -- if done
-- are thrown away and not linked into the IL tree.  If the flag is TRUE, the
prototype instantiations are done *and* preserved, and

* | The ``a_template`` entry for each template poimts to IL for its prototype
    instantiation: a class type for a class template, a routine for a function
    template, or a variable for a variable template or static data member
    template.
* | The ``a_template`` entry points to an entry of type ``a_template_decl``,
    which provides information about the template header (source position,
    parameter list).  The information on parameters is given by a list of
    entries of type ``a_template_parameter``.  (The pointer is NULL for
    a_template entries for member functions, static data members and member
    classes nested in class templates.)

Note that when ``prototype_instantiations_in_il`` is TRUE and
``nonclass_prototype_instantiations`` is FALSE only class prototype
instantiations are included in the IL.

While the front end is processing templates, it is convenient for it to be able
to generate IL versions of entities (like class members) that are not "real,"
i.e., that exist only as part of some template abstraction that will never be
seen by the back end.  For those cases, it is helpful to be able to represent
types and constants which are not known but which nevertheless have a certain
identity, e.g., they're derived from template parameters of a current template.
The ``tk_template_param`` type and the ``ck_template_param`` constant are used
for those cases.  Likewise, a special ``a_template`` entry is created to
represent the unknown value of a template template parameter inside a prototype
instantiation.  These entities will appear in the IL tree only inside prototype
instantiations.

C++/CLI generics are represented as templates with an extra flag
(``is_generic_definition`` in both class types and routines).  They can also
have additional constraint information, provided by entries of type
``a_generic_constraint`` and ``a_generic_constraint_clause``.

Macros
======

If ``RECORD_MACROS_IN_IL`` is set to TRUE, entries of type ``a_macro`` are
created and added to the ``macros`` list of the IL header.  The entries
contain a textual version of the macro definition (e.g., a string like
"``#define x(a) a+1``"), except for predefined macros like ``__LINE__`` and
``__FILE__`` whose replacement text varies from one invocation to the next.
The entries for these macros contain an empty (zero-length) text string
instead of a fixed definition.  If a macro is redefined (to a different
string), there is another macro entry for the new definition.  Macro
entries are also used to represent ``#undef`` directives.

The front end can also be configured to maintain detailed information regarding
macro invocations.  For instance, it is possible to determine the original
source position of text that appears in the expansion of a macro invocation.
This extra position information can be used to produce more-detailed diagnostic
messages regarding such text, and source analysis tools can more accurately
identify the positions and ranges associated with declarations, expressions,
and statements.  It is also possible to determine the chain of macro
invocations that led to a particular macro expansion or to obtain a macro
invocation tree (similar to a function call tree) for an entire translation
unit.

If ``FULLY_RESOLVED_MACRO_POSITIONS`` is TRUE, ``a_source_position`` has an
extra sequence/column pair, ``orig_seq`` and ``orig_column``.  For text
appearing directly in the source, both pairs will indicate the same position.
For text that occurs in a macro expansion, however,
``orig_seq``/``orig_column`` will be the source position at which the text
originally appeared before being copied into the macro expansion -- either in
the definition of some macro or in the argument list of the top-level macro
invocation.  (Regardless of the value of ``FULLY_RESOLVED_MACRO_POSITIONS``,
the ``seq``/``column`` pair for all positions in a macro expansion indicate the
initial position of the top-level macro invocation.)

If ``MACRO_INVOCATION_TREE_IN_IL`` is set to TRUE, a flattened macro
invocation tree is kept in the file scope memory region.  Because
variably-sized data is awkward to represent in the IL, the list of macro
invocation records is broken into fixed-size blocks of type
``a_macro_invocation_record_block``, each containing
``MACRO_INVOCATIONS_PER_BLOCK`` records, and these blocks are organized
into a binary tree to reduce the cost of accessing an arbitrary invocation
record.

There is one record in the tree (of type ``a_macro_invocation_record``) for
each macro invocation that was performed.  Each such record indicates the
parent invocation, the macro that was invoked, and the (original) source
position of the macro name that introduced the invocation.  Furthermore, if
``EXTRA_SOURCE_POSITIONS_IN_IL`` is TRUE, the record will contain the
(original) ending position of the macro invocation (the last character of the
name of an object-like macro or the closing parenthesis in the invocation of a
function-like macro).

In addition, when ``MACRO_INVOCATION_TREE_IN_IL`` is TRUE,
``a_source_position`` contains the index of the macro invocation record in
whose expansion that position occurs (or ``NO_PARENT_MACRO_INVOCATION`` if the
position refers to text that appears directly in source code).

The ``macro_context`` field in ``a_source_position`` and
``parent_macro_index`` in ``a_macro_invocation_record`` enable
reconstruction of the stack of macro invocations that resulted in the text
at that position.  Each macro invocation record is uniquely identified by
an index value of type ``a_macro_invocation_index``, so the macro
invocation stack can be traced simply by following the chain of
``parent_macro_index`` values.  (An invocation of macro ``C`` is considered
to be a child of an invocation of macro ``P`` if ``C``'s invocation appears
in the argument list or the expansion of that invocation of ``P``.)

For convenience in accessing macro invocation records by index,
``il_def.h`` defines the macro ``set_macro_inv_record_ptr_to_index(``\
*root*\ ``,`` *index*\ ``,`` *mirp*\ ``)``, which sets a pointer *mirp* of
type ``a_macro_invocation_record_ptr`` to the address of the record at the
specified index, given the tree root (which is found in
``il_header.root_macro_invocation_record_block``).

In addition to direct access into the tree by record index, it is possible to
scan the tree sequentially; the records are stored in the same order that the
invocations were performed during preprocessing.  To facilitate reconstruction
of the invocation tree during such a traversal without the necessity of
maintaining a separate invocation stack, the array contains certain
"placeholder" records that do not denote macro invocations.  Such records are
identified by a negative value (other than ``NO_PARENT_MACRO_INVOCATION``) for
the parent invocation.

To understand the use of these placeholder records, consider two records
*M* and *N* reflecting successive macro invocations.  If the parent
invocation of *N* is the same as that of *M*, the two invocations are
siblings and the transition from *M* to *N* implies no change to the
conceptual invocation stack.  If the parent invocation of *N* is *M*
itself, then *N* is a child of *M* (i.e., invocation *M* is assumed to be
pushed onto the stack).  Otherwise, a non-negative parent index in *N* is
assumed to be a single-level pop of the conceptual stack -- that is, the
parent of *N* is the parent of *M*'s parent.  In all these cases, the
invocation records for *M* and *N* will be at consecutive indices.  If, on
the other hand, the parent of *N* is a more remote ancestor of *M*, a
placeholder record will be inserted between *M* and *N* with a negative
parent index whose absolute value is the number of invocations to pop from
the conceptual stack in order to restore that remote ancestor to the top of
the stack.

Attributes
==========

C++11 ("standard"), GNU, and Microsoft ``__declspec`` attributes are
represented in the IL by entries of type ``an_attribute``.  If such attributes
appear on a declaration, they are pointed to by the ``attributes`` field of the
corresponding entry's source correspondence; otherwise, a dedicated attributes
field is used (e.g., in ``a_param_type``).  When
``RECORD_UNRECOGNIZED_ATTRIBUTES`` is TRUE, unrecognized attributes are
recorded in the IL with kind ``ak_unrecognized`` (when the macro is FALSE,
unrecognized attributes are not recorded and a warning is issued).  All
attributes specified in the same "group" point to an entry of type
``an_attribute_group``.  Empty groups contain a pseudo-attribute of kind
``ak_empty_attr`` (or more than one, for a GNU construct like
"``__attribute((,,,))``").

If an attribute includes an argument list, the corresponding attribute entry
points to a list of ``an_attribute_arg`` entries.  An empty argument list -- as
in ``__attribute((nonnull()))`` -- is represented with an argument entry of
kind ``aak_empty``.  Otherwise, the entries in the argument list represent a
constant (``aak_constant``), a type (``aak_type``) or an uninterpreted token
(``aak_token`` or ``aak_raw_token``).  ``aak_token`` entries represent single
tokens that may be followed by other arguments separated by a comma.  In
contrast, ``aak_raw_token`` entries represent one or more tokens including the
commas; a sequence of ``aak_raw_token`` entries is always followed by a
terminating ``aak_empty`` entry (used primarily to record the position of the
token that follows in the source -- this is useful for diagnostic purposes).
If an unrecognized attribute is followed by a non-empty argument list, that
list consists of ``aak_raw_token`` entries (including entries for any commas in
the list).  E.g., the unrecognized C++ attribute ``[[ unrecog(x, y) ]]`` has
three ``aak_raw_token`` argument entries for "``x``", "``,``", and "``y``"
(followed by a ``aak_empty`` entry).

Microsoft attributes (delimited by single brackets) are represented in the IL
by entries of type ``an_ms_attribute``.  Depending on the configuration of the
front end, a given Microsoft attribute may be either "recognized" or
"unrecognized".  All attributes include a string representation of the
attribute from the source.  In addition, recognized attributes include an
argument list representing the arguments that were specified in the attribute
reference.

Microsoft attributes that apply to function parameters are pointed to by the
param type entry.  All other attributes are pointed to by the ``ms_attributes``
field of an IL scope entry.  Attributes that apply to a particular entity
include a pointer (see the ``entity`` field) to that entity.  The entity does
not point back, but rather has its ``has_associated_attribute`` flag in its
source correspondence set.

Properties
==========

Microsoft properties come in two kinds:

* | Old-style properties indicated by the ``__declspec(property(``...  ))
    attribute.  The accessor functions are only loosely associated with the
    property, by being named in the attribute.
* | New-style properties in C++/CLI, which begin with the "``property``"
    context-dependent keyword.  The accessor functions are declared as part of
    the property declaration, in a form that looks vaguely like a nested class.

In both cases, the IL has a description of the property declaration, so that is
retained in something close to the source form (that information is held in
``a_property_or_event_descr`` entry).  However, also in both cases, references
to the properties are expanded by the front end, so they are not retained in
source form.  A bit of source code like "``p``\ ``+=``\ ``1``" might be
expanded into a series of calls something like "call the get accessor for
``p``, call ``operator+`` on the fetched value and 1, and call the put accessor
to store the computed value." The generated IL contains some tags on various
nodes in the expansion that help source-analysis code recover the original
meaning, though in a cumbersome way.

C++/CLI events are very similar to the new-style properties, in representation
and in the fact that references are expanded by the front end.

Microsoft ``__if_exists`` blocks
================================

Representing ``__if_exists`` (and ``__if_not_exists``) blocks in the IL is
problematic because, although the Microsoft documentation describes the
contents of an ``__if_exists`` block as a statement, the Microsoft compiler
actually accepts any fragment of a statement or declaration in the block (as
does our front end in Microsoft mode).

The front end provides a mechanism that can represent ``__if_exists`` blocks
that appear in class definitions, and which surround complete declarations of
the class.  When an ``__if_exists`` appears in a valid location in a class
definition, an IL entry (``an_ms_if_exists``) and source sequence entry are
created for the start and end of the block.  ``__if_exists`` blocks in other
scopes are not represented in the IL (but are still included in the textual
representation of templates).  If an ``__if_exists`` appears in an invalid
context in a class definition, an error is issued.

IL entries are only created for ``__if_exists`` blocks that appear in the
prototype instantiation of class templates and classes nested within class
templates.  In non-template contexts, the ``__if_exists`` is simply evaluated
when it is encountered.

IL Use Within the Front End
===========================

Some aspects of the IL are used only during the front end processing and are
not seen by back ends.

Most of the primary table types have a kind used to represent errors (e.g., an
error constant, an error expression).  These are used to replace parts of the
IL tree that contain errors.  Since the back end is not run if there are
errors, a back end will never see these kinds of entries.

There are also a few table kinds used for unknown entities (not known yet, but
not an error), for example the unknown type kind ``tk_unknown``.  These are
replaced by actual kinds by the time a back end sees the IL.

Constant expressions are generally folded to a single constant value and
the front end uses that folded value for its processing.  In scanning
``1+2``, for example, the ``1`` and ``2`` will be folded into a constant
entry for ``3``, and that value will be used internally. The original
expression that was folded is generally preserved via the ``expr`` field of
``a_constant``.

.. _il-extension-guide:

How To Extend the IL
====================

Extending the intermediate language involves making a coordinated set of
changes in several files.  Three kinds of changes are discussed below:

* | adding a field to an existing IL entry (typically quite straightforward),
* | adding a new operator kind (usually also straightforward), and
* | adding a brand new IL entry (only a little more complicated).

Note, however, that what is covered is the typical case.  There may be
additional complications.  And, of course, nothing of a general nature can be
said about how a field or entity is set and used or how it interacts with other
parts of the IL.

Adding a Field to an Existing IL Entry
--------------------------------------

This is what you need to do to add a field *f* of type *T* to existing IL
entry *E* with ``an_il_entry_kind iek_``\ *e*:

.. list-table::

   * - | in ``il_def.h``:
     - | Add *f* to *E*.
   * - | in ``il_alloc.c``:
     - | Initialize field *f* in the routine that allocates *E* objects.
   * - | in ``il_display.c``:
     - | Add code to display the value of *f*.  This will typically appear
         in a function called ``disp_``\ *e*, and the display will depend
         on the type *T*.  For example, ``disp_boolean`` will be called for
         a flag, ``disp_ptr`` will be called if *T* is of type
         pointer-to-IL-entry, and so forth.

and in addition, if *T* is of type pointer-to-IL-entry:

.. list-table::

   * - | in ``walk_entry.h``:
     - | In ``WALK_ENTRY_ROUTINE_NAME``, there will be a ``case`` clause
         for ``iek_``\ *e* within the main ``switch`` statement.  Here you
         will usually need to add a call to ``walk_ptr`` or ``remap_ptr``.
         (The latter is typically used if *f* is a secondary reference to
         the object it points to, but ``walk_ptr`` can be used in any
         case.) There are some special cases to look out for, however.  For
         example, if *f* points to a linked list, instead of ``walk_ptr``
         call either ``walk_list`` (if ``*``\ *T* uses a ``next`` pointer)
         or ``walk_list_on_link_field``.

If *f* is an addition to the ``a_type`` entry, update ``traverse_type_tree``
accordingly.

If *f* is an addition to the ``an_expr_node`` entry, update ``traverse_expr``
accordingly.

If *f* is an addition to the ``a_statement`` entry, update
``traverse_statement`` accordingly.

If *f* is an addition to the ``a_dynamic_init`` entry, update
``traverse_dynamic_init`` accordingly.

If *f* is an addition to the ``a_constant`` entry in its function as the
description of an initializer, update ``traverse_constant`` accordingly.

Adding a New Operator
---------------------

Most expression nodes (``an_expr_node``) represent operations
(``enk_operation``) like additions, assignments, calls, casts, etc.  Each of
these operation kinds is identified by an enumeration constant (``eok_add``,
``eok_call``, ...).  To add a new operation kind, it is not sufficient to add
another constant; the following updates are also required:

.. list-table::

   * - | in ``il_def.h``:
     - | Add a string constant to ``db_operator_names``.
   * - | in ``il_display.c``:
     - | Add a case for the new operator in ``disp_expr_operator_name``.
   * - | in ``cp_gen_be.c``:
     - | Update ``generated_precedence`` to cover the new operator.
   * - | in ``il.c``:
     - | Update ``lvalue_rvalue_test`` and ``operation_type_kind``, and if the
         operator has side effects, also ``operation_has_side_effects``.

Furthermore, if the operator may produce an lvalue (``is_lvalue`` is TRUE) or
xvalue (``is_xvalue`` is TRUE), additional functions must know about the new
operator kind:

.. list-table::

   * - | in ``il.c``:
     - | ``is_rvalueable_node``, ``node_does_fetch``, and
         ``operator_takes_lvalue_operand``.
   * - | in ``exprutil.c``:
     - | ``conv_prvalue_expr_to_glvalue``, and (if ``is_rvalueable_node``
         indicates that the operator is not "rvalueable")
         ``conv_glvalue_expr_to_prvalue``.

Finally, if the operator is an addressing operator, a few other routines
require updating:

.. list-table::

   * - | in ``folding.c``:
     - | ``constant_glvalue_address`` and ``constant_prvalue_pointer``.
   * - | in ``il_walk.c``:
     - | ``traverse_addressing_subtree``.

Adding a new expression node kind (new ``enk_...``\ variant) is harder.  That
requires looking at all code that handles expressions, in particular switch
statements that case out on the expression kind, and making appropriate
changes.  If the new variant includes an embedded type, be sure to update
``examine_expr_for_dependent_type``.

Adding an IL Entry
------------------

This is what you need to do to add an IL entry of ``struct`` type *X*,
where *X* is a name with the form ``a_``\ *xxx*:

.. list-table::

   * - | in ``il_def.h``:
     - | Add the definition of *X*.  Add ``iek_``\ *xxx* to the enumeration
         ``an_il_entry_kind``.  Update the initialization of
         ``il_entry_kind_names`` and ``sizeof_il_entry``.
   * - | in ``walk_entry.h``:
     - | Add a ``case`` clause for ``iek_``\ *xxx* in
         ``WALK_ENTRY_ROUTINE_NAME``.  Add calls to process orphans in
         ``WALK_ORPHANED_ENTRY_ROUTINE_NAME``.
   * - | in ``il_walk.c``:
     - | Add calls to process orphans in
         ``remap_first_ptr_of_orphaned_file_scope_entry_array`` and in
         ``remap_last_ptr_of_orphaned_file_scope_entry_array``.
   * - | in ``il_alloc.c``:
     - | Add an ``alloc_``\ *xxx* routine.  If the entity should always be
         allocated in the file scope memory region, call ``alloc_il``; to use
         the current memory region, call ``alloc_cil``.  (Do not use
         ``alloc_fe`` for entities in the IL.) If the entity is a supplement to
         some other entry (to keep down the size of the containing entry), the
         space for the supplement should be allocated when the containing entry
         kind is set to that variant.  See examples in ``set_type_kind``.  In
         such cases, you should also change the copy routine for the containing
         entity so that it copies the supplement as well.  See ``copy_type``
         and ``copy_node`` for examples.  If you want to track the number of
         entities of type *X* that are allocated and how much space they use,
         define a static variable ``num_``\ *xxx*\ ``es_allocated``, initialize
         it in ``il_alloc_init``, register it in ``il_alloc_one_time_init`` in
         case precompiled headers are used, increment it in ``alloc_``\ *xxx*,
         and display it by adding code to ``show_il_alloc_space_used``.
   * - | in ``il_display.c``:
     - | Add a ``disp_``\ *xxx* routine.  Add a ``case`` clause for
         ``iek_``\ *xxx* in ``disp_entry``.  If the entity is a supplement,
         add its kind to the list at the beginning of ``disp_entry`` (a
         list of entity kinds for which the entity is displayed with the
         containing entity), and put the call of ``disp_``\ *xxx* in the
         clause for the containing entity.
   * - | in ``il.c``:
     - | If the entry might be an orphan (allocated in the file-scope memory
         region, pointed to from something in a function-scope memory region,
         but not pointed to from anything in the file-scope memory region), add
         the entry kind as a case in
         ``f_possibly_add_orphaned_file_scope_il_entry``.

This is what is required in the general case, but there are potentially many
other issues to consider.  For example, if *X* has a substructure of type
``a_source_correspondence``:

..  list-table::

   * - | in ``il_alloc.c``:
     - | Be sure ``alloc_``\ *xxx* calls ``set_default_source_corresp``.
         Add a ``case`` clause for ``iek_``\ *xxx* in
         ``source_corresp_for_il_entry``.
   * - | in ``il_display.c``:
     - | Add a ``case`` clause for ``iek_``\ *xxx* in ``disp_ptr``.

If *X* is added to ``a_class_type_supplement``, and the field is one that is
meaningful even if the class does not have a definition:

.. list-table::

   * - | in ``il.c``:
     - | If you use "needed" flags (``MAINTAIN_NEEDED_FLAGS`` is TRUE), modify
         ``turn_class_definition_into_declaration`` to copy *X* when clearing
         the class type supplement.

.. [#f1] The obvious exception to that -- the reference from the interface to a
         routine down to the function scope that defines its implementation --
         is handled as a special case.  What is stored in the interface is not
         a pointer, but rather a function definition number, which can be
         looked up in ``function_def_table`` in ``il_header`` to get a pointer
         to the scope entry if it is in memory.
.. [#f2] Why do that? Because we need the type of the entities to check other
         declarations of the same name and to check calls to external entities
         that are functions.
.. [#f3] One exception: GNU and Microsoft allow the alignment of a typedef to
         be set explicitly, and that alignment is recorded in the associated
         ``tk_typeref`` entry.  For this reason, the macro
         ``alignment_of_type`` should always be used to retrieve the alignment
         of a type.
.. [#f4] A Microsoft ``__interface`` type is treated as a special kind of C++
         ``struct`` type.  A C++11 lambda closure type is a generated C++
         class type.
.. [#f5] One compelling reason: constants used as part of aggregate constants
         are linked together using their ``next`` pointers, so a unique copy of
         the constant is necessary for each initializer list.
.. [#f6] Because dynamic initialization entries can do destruction without
         initialization, there is a ``dik_none`` value for ``kind`` to indicate
         that no initialization should be done.
.. [#f7] There are some additional kinds of bindings generated by IL lowering;
         see ``il_def.h`` for details.
.. [#f8] Following the implementation of C++11 value categories in version
         4.8, the front end has tried to use the C++11 terms in general
         internally.  That caused a lot of renaming of routines and fields; for
         example, ``conv_lvalue_to_rvalue`` became ``conv_glvalue_to_prvalue``.
         Ugly until you get used to it, but very helpful in reminding you to
         consider xvalues when you write code that deals with expressions.
