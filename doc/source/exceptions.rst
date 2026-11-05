==================
Exception Handling
==================

The high-level representation of exception handling in the IL closely follows
the C++ language specification.  The data structures involved are defined in
``il_def.h``, and the routines to manage declarations and other support are in
``decls.c``, ``statements.c``, and ``expr.c``.

A global variable, ``exceptions_enabled``, controls whether exception
processing is done.  An error is issued if the flag is FALSE and the source
program attempts to use a syntactic feature that pertains to exception
handling.  The default value of the ``exceptions_enabled`` flag is configurable
(see ``DEFAULT_EXCEPTIONS_ENABLED`` in ``lang_feat.h``).  The value may be
toggled by a command-line option (``-x``), except that its value may be TRUE
only in C++ mode and only when cfront-compatibility mode has not been selected.

Any actual implementation of exception handling will require a tradeoff between
portability and efficiency.  The high-level representation of exception
handling (that is, the representation of the "unlowered" IL) is not explicit
about a number of details that an actual implementation will have to deal with,
notably finding the right handler, copying the ``throw`` object to the right
handler parameter, and calling the right set of destructors at the right time.
Therefore, though a portable implementation of exception handling is provided
(see ``lower_eh.c`` and routines provided for run-time support), back ends may
choose to replace it with an implementation adapted to the specific support
available on the target platform.  IL lowering can instead be configured to do
partial lowering or no lowering of exception handling features.

This chapter describes the high-level support for exceptions and then
summarizes the main features of the portable implementation.

Exception Specifications
========================

An exception specification that appears on a function declaration is
represented by an entry of type ``an_exception_specification``.  The
function refers to it by means of the ``exception_specification`` field of
its routine type supplement.  When the function is declared with no
exception specification (meaning that *any* exception might be thrown), the
``exception_specification`` pointer will be NULL.  A dynamic ``throw``
specification entry itself has a pointer to a linked list of entries of
type ``a_throw_spec_type_entry``, each of which indicates a type of
exception that will be thrown from the given routine; that pointer is NULL
to indicate that *no* exceptions will be thrown.  A ``noexcept`` specifier
points to a constant representing its operand or NULL if there is no such
operand.

``scan_exception_specification``, which is called from ``function_declarator``,
scans an exception specification and allocates a ``throw`` specification entry
and its associated list of types, if any.  Subsequently (i.e., after the
routine entry has been identified) the exception specification is either added
to the routine type supplement or checked against a prior declaration (see
``check_exception_specification``).

The ``used_in_exception`` flag is set for any type that appears in an exception
specification (as well as for any type that is the type of a ``throw``
expression or appears in the exception declaration of a handler).  Some
implementations of exception handling may use this flag in generating special
code for run-time type identification.  In addition, if such a type is (or
"contains") a class or enumeration type, that class or enumeration type must be
externally linked.  See ``set_used_in_exception_or_rtti_flag`` (in
``types.c``).

``try`` Blocks
==============

A ``try`` block is represented in the IL by a statement of kind
``stmk_try_block``, which points to a compound statement block and a linked
list of handlers.  The latter are entries of type ``a_handler``; each points to
a (possibly unnamed) parameter (the local variable to which the ``throw``
object will be copied when the handler is invoked) and to an ``stmk_block``
statement, the body of the handler; for a default handler (i.e., the
``catch(...)`` case) the parameter pointer in the handler entry is NULL.

A ``try`` block is scanned by ``try_block_statement`` (in ``statements.c``).
First ``compound_statement`` is called to scan the part of the ``try`` block
that precedes the first handler declaration.  Then ``handler_declaration`` (in
``decls.c``) is called for each ``catch`` clause.

``handler_declaration`` scans the exception declaration (calling
``decl_specifiers`` and, if a named parameter is present, ``declarator``).
Except for the default handler case, it creates a ``variable`` entry to
represent the parameter, whether it is explicitly declared or not.  It also
does various checks to assure that the parameter's type is valid (e.g., that it
is not an incomplete type) and calls ``set_used_in_exception_or_rtti_flag``;
then it calls ``type_masks_handler_param_type`` (in ``types.c``), to assure
that it is not masked by a previous handler declaration in the same ``try``
block.

Finally, ``compound_statement`` is called to scan the handler's body.  Special
checking is done to assure that no label defined within the handler is
referenced by a ``goto`` from outside the handler.

in C++/CLI mode, there may be a "``finally``" block at the end, either in
addition to or in place of the catch clauses.  It provides code that is
always executed when the try block is exited, whether normally, via a
``goto`` or ``return``, or via a thrown exception.

``throw`` Expressions
=====================

A ``throw`` expression is scanned by ``scan_throw_operator`` (in ``expr.c``).
An ``enk_throw`` expression node is produced, which points to a dynamic
initialization entry that describes the object to be thrown (or rather, how to
copy it to the space for the object allocated by the run-time).
``set_used_in_exception_or_rtti_flag`` is called for the type.  A "rethrow",
where no ``throw`` object is specified (i.e., ``throw;``), is represented by an
``enk_throw`` expression with a NULL pointer.

Portable Implementation
=======================

One exception handling implementation provided in this release is a portable
implementation that provides complete EH support while requiring few changes to
existing back ends.  Because this is a portable implementation there is a
significant impact on the performance of the generated code when exception
handling is enabled.

A walk-through of an EH example
-------------------------------

What follows is a rather contrived example that provides an overview of how the
compiled code and the run-time system cooperate in throwing and handling
exceptions.  It is intended as an introduction, with a more detailed discussion
of some of its features in subsequent sections.

Here is the example (with the numbers to the right corresponding to the text
that follows):

.. code:: c++

   #include <stdio.h>
   struct A { ~A() { } };
   struct B : public A { };
   struct C { };
   void f(int i) throw(A) {                       // 2, 3, 4
     A  a1;                                       // 5
     if (i==0) throw a1;                          // 6
     B  b1;
     if (i==1) throw b1;
     C  c1;
     if (i==2) throw c1;
   }
   int main() {
     for (int i = 0; i < 3; ++i) {
       printf("i=%0d\n", i);
       try {                                      // 1
         f(i);
       }
       catch (A a) { printf("Caught an A\n"); }   // 7, 8
       catch (...) { printf("Caught something else\n"); }
     }  /* for */                                 // 9
   }

#. Each time the ``for`` loop in ``main`` is executed and control enters
   the ``try`` block, prologue code is executed, which includes pushing an
   entry onto the "EH stack" to record that a ``try`` block has been
   entered. (The EH stack is the principal data structure used to track the
   dynamic context in which exception handling events occur.)

#. Each time ``f`` is called, prologue code for the function pushes another
   entry onto the EH stack to record that a function with exception
   specifications has been entered. This entry provides the run-time system
   with a list of the types that may be thrown from ``f``.

#. Moreover, since ``f`` contains objects (namely, automatic variables with
   destructors) that may need to be cleaned up if an exception is thrown
   during its execution, an additional entry is pushed onto the EH stack
   for object cleanup. [#f1]_ The array it points to is updated each time
   an object that is eligible for clean-up is constructed.

#. The first clean-up region of ``f`` is entered, and global variable
   ``__eh_curr_region`` is updated accordingly. This routine has three such
   regions -- the region in which no objects require clean-up, the region in
   which ``a1`` requires clean-up, and the region in which both ``a1`` and
   ``b1`` require clean-up. [#f2]_

#. When ``a1`` is constructed, the second clean-up region is entered. The
   address of ``a1`` is entered in the "object address table".

#. The processing for the original source expression "``throw a1``"
   involves three steps:

   * | ``__throw_setup`` is called to record information about ``a1`` (most
       importantly, its type) and to return a pointer to a piece of storage
       into which it may be copied. [#f3]_

   * | The compiled code makes a copy of ``a1`` to pass to
       ``__throw``. Note that if class ``A`` had a copy constructor, the
       generated code would include a call of ``__exception_started`` prior
       to making the copy of ``a1``.

   * | ``__throw`` is called. First it walks the EH stack to identify the
       action that is required -- in this case, it locates a handler that
       can catch an ``A`` object. It then makes a second pass through the
       EH stack to do the appropriate clean-up, which in this case involves
       calling the destructor for ``a1`` and popping a couple of entries
       off the EH stack. The actions performed by ``__throw`` may entail a
       good deal more complexity, as described in greater detail later.

#. After the clean-up is complete, control transfers back to the ``try``
   block in ``main``, and specifically to the first handler (since it is
   the one that catches an ``A`` object), and then "``Caught an A``" is
   printed out.

#. The epilogue code of the handler involves a call to
   ``__free_thrown_object``, to deallocate the storage into which ``a1``
   was copied.

#. As the ``try`` block is exited, the EH stack entry for the current
   ``try`` block is popped, restoring the state to what it was originally,
   before the try block was entered.

On the second iteration of the ``for`` loop in ``main``, the same sequence of
events takes place, except for two things.  First, in function ``f``\ ``b1`` is
also constructed, so that the third clean-up region is entered and the address
of ``b1`` is recorded in the object address table (along with that of ``a1``);
that means that two objects are eligible for clean-up when an exception occurs.
Second, ``__throw`` must take into account that ``A`` is an accessible base
class of ``B`` in locating the handler that can catch ``b1``.

Finally, on the third iteration of the loop, ``__throw`` detects a violation of
the exception specification for function ``f`` -- ``c1`` is the thrown object
this time, but ``C`` is not on the list of types that the function is allowed
to throw.  As a result, ``unexpected`` is called, which results in a call to
``terminate``.

Run-Time Support
----------------

The EH Stack
^^^^^^^^^^^^

A principal data structure used in coordinating the compiled code and the
run-time is the "EH stack".  It maintains information about the dynamic context
in which exception handling events occur.  The EH stack is a linked list of
structures, and a new entry is pushed onto the stack each time a ``try`` block
is entered, each time a function with an exception specification is entered,
and each time a function is entered for which some kind of object clean-up must
be done if an exception is thrown.

Code generated for a ``try`` block
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When a ``try`` block is entered, a ``try`` block entry is pushed onto the EH
stack.  The ``try`` block entry contains

* | a ``setjmp`` buffer that will be used for transfer of control back to the
    ``try`` block when an exception is thrown;
* | a list of the ``catch`` clauses associated with the ``try`` block;
* | other status information used to record the current state of the ``try``
    block.

The state information in the ``try`` block is initialized so that the run-time
knows that the ``try`` section of the block (as opposed to one of the handlers)
is currently being executed.  When generating code for exception cleanup it is
sometimes necessary for the front end to generate ``try`` blocks that do not
appear in the original program.  These are known as "internal" ``try`` blocks
and are distinguished from normal ``try`` blocks by the fact that the pointer
to the list of ``catch`` clauses is null.

Prologue code for a function with an exception specification
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When a function with an exception specification is entered, a ``throw``
specification entry is pushed onto the EH stack.  This entry provides a list of
the types that may be thrown (directly or indirectly) by the function.  It is
removed from the EH stack when the function returns.

Prologue code for a function that may need clean-up if an exception is thrown
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When a function constructs objects that may need to be cleaned up in the event
of an exception, its prologue code pushes a function clean-up entry onto the EH
stack.  Note that a given function may push both a ``throw`` specification
entry and a function clean-up entry.

A function clean-up entry will be pushed on the stack if the function creates
any automatic objects (including temporaries) of classes with destructors; one
is also generated when an object may be dynamically allocated using ``operator
new`` and the front end is configured to generate the ``operator new`` call
outside the constructor.

The function clean-up entry contains

* | a pointer to an array of region entries that are used to describe the
    clean-up regions within the function
* | a pointer to an array table that provides additional information for
    certain region entries
* | a pointer to an object address table used to determine the address of
    objects referenced by the region and array tables

A region is associated with a specific sequence of clean-up actions that would
be required should an exception occur.  A new region is typically started
whenever a destructible object comes into or goes out of scope.

The global variable ``__eh_curr_region`` records the region number currently
being executed.  It is saved each time a new function entry or ``try`` block is
pushed on the stack and restored when the entry is popped.

The region entries contain the information needed to identify the object to be
cleaned up and the kind of clean-up action required.  The run-time must be able
to determine the address of each object for which some kind of clean-up must be
done.  Ideally this would be done by putting stack offsets in the region
entries.  This can't be done in the portable implementation, and so instead the
region entries contain an index into the object address table.  The object
address table is allocated on the stack because its contents vary with each
invocation of the function.  As each region is entered, the appropriate entry
in the object address table is filled in with the address of the object to be
cleaned up.  The address of the object address table is recorded in the
function entry on the EH stack.

The region entries have been designed to be as compact as possible.  Some
unusual circumstances require additional information not normally available in
the region entry.  The additional information is needed when the region entry
describes an array or when the region entry describes a new allocation of a
class object whose ``operator delete`` function is of the two-operand variety.
In these cases the region entry contains an index into an array supplement and
the array supplement pointer is stored in the function entry on the EH stack.

Throwing an exception (generated code)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When an exception is thrown, the run-time routine ``__throw_setup`` is called
to provide information to the run-time system about the object being thrown and
to allocate space into which the thrown object may be copied.  A pointer to
this space is returned to the caller (i.e., the compiled code), which is
responsible for actually making the copy of the object.  If copying the object
requires calling a copy constructor, ``__exception_started`` is called after
the evaluation of the object to be thrown, but before the copy constructor is
called.  Once the object has been copied the run-time routine ``__throw`` is
called to complete the throw processing.  If the thrown object requires
destruction, ``__throw_setup_dtor`` is called instead of ``__throw_setup``.  If
the thrown object is a multi-level pointer, ``__throw_setup_ptr`` is called
instead of ``__throw_setup``.

Throwing an exception (``__throw_setup``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When ``__throw_setup`` is called, an entry is pushed onto the throw stack
(which is distinct from the EH stack).  The throw stack contains information
about each of the thrown objects and is needed because throws may be nested
(i.e., a throw may occur while in a handler reached as the result of an earlier
throw).  ``__throw_setup`` records the information provided by the caller in
the throw stack entry.  This includes information about the type of the thrown
object.

The run-time system requires information about the types of objects that are
thrown and caught and also for types used in exception specifications.  The
term "``typeinfo``" is used to refer to this type description information.
[#f4]_ It is not necessarily possible to generate only one ``typeinfo`` record
for a given type.  [#f5]_ Consequently it must be possible to determine that
two ``typeinfo`` records refer to the same type.  This is done by having each
``typeinfo`` point to a "unique ID object".  The unique ID objects are
generated as tentative definitions to avoid potential multiple definition
problems.  Two ``typeinfo`` records refer to the same type if they point to the
same unique ID object.  The ``typeinfo`` information includes a list of the
base classes of the type.  The base class list includes all direct base classes
as well as all virtual base classes, whether direct or indirect.

When checking exception specifications and looking for a ``catch`` clause that
can catch the thrown object, it is necessary to determine whether a given type
is a base class (and an accessible one) of the thrown type.  This is done by
traversing the ``typeinfo`` base class list.  The base class list includes
information about accessibility and ambiguity, which allows the runtime to
ignore non-public and ambiguous base classes.

In front end versions preceding 2.29, the runtime routine ``__throw_alloc`` was
called instead of ``__throw_setup``.  It had a similar function and a similar
interface, with one additional parameter, which (when non-NULL) was a string
indicating the accessibility for each base class, computed by the front end at
the point of the throw.  This changed because the C++ language changed: As of
March 1995, the rules for ``throw`` were changed to allow catching only public
base classes, which eliminates the need for determining any special
accessibility at the point of throw.

Marking the initiation of the exception (``__exception_started``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

An exception is considered "uncaught" after the expression in the throw
statement has been evaluated.  If a copy constructor must be called to copy the
thrown object into the temporary copy used by the EH runtime, the copy
constructor must be called while the exception is considered uncaught.  In
other words, if an exception is thrown during the evaluation of the expression
in the throw, that exception is handled normally.  If, however, an exception is
thrown from a copy constructor called to copy an object to the temporary used
by the EH runtime (and control is transferred out of the copy constructor
because of the exception) ``terminate()`` must be called instead.

``__exception_started`` is called to mark the point at which an exception is
considered uncaught.  When the copy of the thrown object can be made without
calling a copy constructor ``__exception_started`` is not called, and the
exception is considered uncaught at the point at which ``__throw`` is called.

Throwing an exception (``__throw``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

After the compiled code copies the thrown object into the space designated by
``__throw_setup``, it issues a call to ``__throw`` to complete the processing.

``__throw`` makes two passes through the ``try`` stack.  The first pass
determines what should occur as a result of the throw -- transferring to a
specific handler, calling ``unexpected`` because an exception specification was
violated, or calling ``terminate`` (for any of several reasons).  Once the end
result of the throw has been determined, a second pass is made through the EH
stack entries to perform any clean-up actions that are required.

During the first pass through the EH stack
``check_exception_type_specifications`` is called for each ``try`` block entry
and each ``throw`` specification entry.  Only ``try`` blocks that are executing
the ``try`` portion of the block (as opposed to a handler) are eligible to be
the destination of a ``throw``.  ``try`` blocks that are currently in handlers
are simply ignored.  The search is terminated when a matching ``catch`` clause
is found or a violated exception specification is detected.  When it is a
matching ``catch`` clause that is found, a pointer to the thrown object is
passed to the routine so that it may be adjusted for any derived-to-base-class
conversions that may be needed.  If the matching ``catch`` clause is associated
with an internal ``try`` block, the search of the EH stack continues to
determine if a normal ``try`` block can be found.  The search for a normal
``try`` block that with a matching handler is used to determine whether
``terminate()`` should be called.

According to the standard it is implementation-defined whether the stack is
unwound before ``terminate()`` is called.  The runtime can be configured to
select either of the possible behaviors by setting the
``UNWIND_STACK_BEFORE_CALLING_TERMINATE`` configuration flag.  If this flag is
not set, ``terminate()`` would be called now if no matching handler was found.

In the second pass only the function clean-up entries are processed.  The
function ``cleanup`` is called for each such entry.  ``cleanup`` processes the
region entries associated with the function starting with the region number
specified by the caller.  Each region entry contains a "flags" field that
controls how the region information is interpreted.

* | If the "conditional flag" bit is set, the region entry describes an object
    that is conditionally constructed and must only be destroyed if a variable,
    whose address is determined from the next element in the region array,
    indicates the object was actually constructed.
* | If the "new allocation" bit is set, the region entry describes an object
    allocated by ``new`` that must be deleted during clean-up.  In this case
    the function pointer in the region entry that normally points to the
    destructor instead points to the delete routine to be called.
* | If the "VLA" bit is set, the region entry describes a variable length
    array, whose size is determined from the next element in the region array.

One of the fields in the region entry is the region number of the next entry to
be processed.  The end of the region list is designated by a reserved value in
the next entry field.

If an exception occurs while constructing or destructing an array, the clean-up
operation must destroy the remaining portion of the partially constructed (or
destructed) array.  To accomplish this there is a special kind of EH stack
entry that is only used by ``vec_new`` and ``vec_delete``.  When this entry is
encountered during the clean-up pass, ``__cleanup_vec_new_or_delete`` is called
to the required clean-up.

After all of the function clean-up entries have been processed, ``__throw``
determines whether there are any clean-up actions required by the ``try`` block
associated with the handler to which control is to be transferred.  If so,
``cleanup`` is called to do the required clean-up operations.

Only after all the clean-up operations have been completed is the action
performed that was determined by the first pass through the EH stack:

* | If an exception specification was violated, ``unexpected`` is called.
* | If no handler was found, ``terminate()`` is called (when the
    ``UNWIND_STACK_BEFORE_CALLING_TERMINATE`` configuration flag is set).
* | Otherwise, control is transferred to the handler.  Global variable
    ``__catch_clause_number`` is set to the sequence number of the ``catch``
    clause to which control is to be transferred, and another global variable,
    ``__caught_object_address``, is set to the address of the object to be used
    to initialize the handler parameter.  The state information in the ``try``
    block entry is updated to indicate that a handler is now being executed.
    Finally, ``longjmp`` is used to branch back to the appropriate ``try``
    block.

In the handler
^^^^^^^^^^^^^^

The ``try`` block code uses ``__catch_clause_number`` to select the appropriate
handler, and then the handler parameter is initialized using
``__caught_object_address``: If the handler parameter is a reference, it is
initialized by setting a pointer to the global variable; otherwise, the object
pointed to by ``__caught_object_address`` is used to initialize the handler
parameter.  ``__exception_caught`` is called after the handler parameter has
been initialized.  This marks the end of the period during which an exception
is considered uncaught.  ``__exception_caught`` is not called by the catch
clause associated with an internal try block.  Instead, an internal try block
exits by calling ``__internal_rethrow``, which calls ``__exception_caught``
before performing the rethrow.

A rethrow from the handler
^^^^^^^^^^^^^^^^^^^^^^^^^^

A rethrow (a ``throw`` with no operand) may be executed anywhere within the
dynamic context of a handler.  This has the effect of doing a nested throw of
the object on top of the throw stack.  Note that it rethrows the thrown object
and not the caught object (the two can be different if a derived-to-base
conversion occurred or if the object was caught using ellipsis).

The handling of rethrow is almost identical to the handling of a normal throw.
The only difference is that the thrown object is not copied.  The throw stack
entry for the rethrow points to the copy of the thrown object made when the
original throw was done.

At the end of a handler
^^^^^^^^^^^^^^^^^^^^^^^

At the end of each handler ``__free_throw_object`` is called to pop entries off
of the throw stack, call the destructor for the thrown object, and to free the
space used for copies of the thrown objects associated with the throw stack
entries being cleared.

Run-Time storage management
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The exception handling run-time dynamically allocates information used for book
keeping (e.g., throw stack entries) and for making copies of thrown objects.
The memory is allocated and freed using a stack discipline.  The run-time
includes a static buffer that is used for this dynamically allocated memory.
If the static buffer is exhausted, additional memory is allocated using
``malloc``.  If the run-time is unable to allocate the memory needed to handle
an exception, ``terminate`` is called.

Non-Portable Implementation
===========================

The non-portable implementation would be more properly described as "turning
off parts of the portable implementation so that a back end can do something
better." EDG does not provide a complete non-portable implementation (in IL
lowering and the runtime), but our customers should be able to write one using
the information provided by the front end.

The portable implementation is also known as the full-lowering implementation,
because it lowers everything related to exception handling to C, leaving
nothing related to exception handling in the IL.  It is enabled by setting
``DO_FULL_PORTABLE_EH_LOWERING`` to TRUE.

The next step down from that is the partial-lowering implementation, which is
enabled by setting ``GENERATE_EH_TABLES`` to TRUE.  In that mode, the data
tables of the portable scheme are still generated, but the executable code is
rendered as exception-handling operations, so that a back end can choose a
different approach (e.g., not using ``setjmp``, not maintaining a separate EH
stack).

The final step down is the no-lowering implementation, which is enabled by
having both ``DO_FULL_PORTABLE_EH_LOWERING`` and ``GENERATE_EH_TABLES`` FALSE.
In that mode, the data tables are not generated either, but the object lifetime
information is preserved so that the back end can use it to generate its own
version of the cleanup tables.

See the :ref:`il-lowering` chapter for details on the constructs
preserved and lowered under each alternative.

.. [#f1] Note that the prologue code for ``f`` pushes two entries onto the EH
         stack, one for its exception specifications and one for object
         clean-up.
.. [#f2] ``c1`` requires no clean-up since ``C`` has no destructor.
.. [#f3] In versions before 2.29, the runtime routine was called
         ``__throw_alloc``.
.. [#f4] This is not the structure called "``type_info``" that is visible to
         the programmer via ``typeid``.
.. [#f5] Unique ``typeinfo`` records can only be generated for classes with at
         least one noninline virtual function (i.e., the same algorithm that is
         used to generate virtual function tables).
