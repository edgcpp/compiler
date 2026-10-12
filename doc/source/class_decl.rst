==================
Class Declarations
==================

All class, struct, and union declarations are scanned in ``decl_spec.c``, and
their definitions, including the declarations of class members, are handled
through a set of routines, most of which are found in ``class_decl.c``.  The
layout of classes is handled in ``layout.c``.  Associated declarations are in
``class_decl.h`` and ``layout.h``.

Declarations of structs and unions in C mode are, for the most part, precisely
compatible with C++ declarations; they just make use of a small subset of the
syntactic options.

C++/CLI managed class add both a number of variant class kinds (collectively
called "managed classes") and a number of variant member kinds (e.g., events).
These are also mostly implemented in ``class_decl.c``.

C++11 lambda expressions implicitly define a closure class type.  While the
top-level class declaration parsing routines (``class_specifier`` and
``scan_class_definition``) are not applicable for parsing lambdas, some other
important routines in this area are reused (see :ref:`lambdas`).

Similarly, C++/CLI delegate definitions implicitly define a ref class type,
which reuses some important routines supporting the ordinary definitions of
class types, but not the routines that parse such definitions.

Overview
========

Class Declarations
------------------

``class_specifier`` (in ``decl_spec.c``) is called from ``decl_specifiers``
when a ``class``, ``struct``, or ``union`` keyword is encountered.  [#f1]_ In
Microsoft mode, this is also done with the ``__interface`` keyword, and in
C++/CLI mode with the "keywords" ``ref class``, ``ref struct``, ``value
class``, ``value struct``, ``interface class``, and ``interface struct``.  A
tag name may be present, and if it is ``declares_something`` is returned
TRUE.  A definition may be present, and if it is ``defines_something`` is
returned TRUE.  In every case, even when there is an error, a pointer to a
``tk_class``, ``tk_struct``, or ``tk_union`` type entry is returned in
``type_ptr``.

The processing of class declarations consists of several steps.  The tag name
(if present) is scanned and looked up in the symbol table (see
``scan_tag_name``).  A declaration may be the declaration of a new class, a
reference to an existing class, or the completion of a declaration whereby an
incomplete class is fully defined; it may also be a specialization of a class
template.  If this is a brand new class, the type entry is allocated for it;
otherwise, the old one is fetched from the symbol entry.

If the declaration does not actually *define* a class, that's about all that
needs to be done.  The type is returned to the caller and declaration
processing continues.

Class Definitions
-----------------

If a class declaration is a class definition -- that is, if the class body is
present -- then ``scan_class_definition`` is called from ``class_specifier``
(it is also called from other places like ``f_instantiate_template_class`` to
create an instance of a template class and ``instantiate_class_template`` to do
prototype instantiation of the class template).  The creation of a class
definition is tracked using type ``a_class_def_state``; this is true for
ordinary definitions appearing in the source, but also for definitions
generated for, e.g., lambda expressions (i.e., closure types).  This structure
records all kinds properties of the definition (e.g., the current accessibility
or whether a member has been seen that requires a nontrivial constructor) and
is passed around to many routines that implement class member declarations.

Most of the tasks performed by ``scan_class_definition`` are done in C++ mode
only:

* | The base class specifiers (if any) are scanned and recorded.
* | The scope stack is pushed for the class (and the scope stack entry is made
    to point to the ``a_class_def_state`` structure for the definition).
* | A loop is entered to scan class members.  In ordinary C this means scanning
    the declarations of fields.  In C++ it means scanning all kinds of member
    declarations.
* | At the top of the loop (in C++ only) an optional access control specifier
    is scanned.
* | When ``using`` appears (or there is only a qualified name and no
    declaration specifiers), a using declaration (or access declaration) is
    scanned.
* | When ``template`` appears ``template_declaration_or_directive`` is called
    to process a member template.
* | Otherwise, ``class_member_declaration`` is called:

  * | A structure of type ``a_member_decl_info`` tracks properties of the
      declaration.  This structure includes a field ``decl_state`` of type
      ``a_decl_parse_state`` (which is the kind of structure used for general
      declaration processing).
  * | ``decl_specifiers`` (in ``decl_spec.c``) is called to scan the
      declaration specifiers.
  * | When there is no declarator following,

    * | if it is a friend class declaration, ``decl_friend_class`` is called
        and declarator processing is skipped.
    * | if it is a nested ``class``, ``struct``, ``union``, or ``enum``
        declaration, declarator processing is skipped.
    * | if it is a nested anonymous union declaration, declarator processing is
        entered (even though the call to ``declarator`` itself is skipped)
        since the rest of the processing is done in
        ``decl_nonstatic_data_member``.
  * | A loop is entered to scan a declarator list.  In cases where only a
      single declarator is permitted, the loop will be exited at the
      appropriate point.
  * | ``declarator`` (in ``declarator.c``) is called to scan a single
      declarator.  If default arguments are encountered when a parameter list
      for a member function is scanned, they will be cached away to be
      rescanned later.
  * | Based on the combined information returned from ``decl_specifiers`` and
      ``declarator``, one of the following is done:

    * | ``decl_member_function`` is called to process a member function
        declaration (static or nonstatic, virtual or nonvirtual).  If the
        function body is present, the tokens are cached so that they can
        scanned later (see ``prescan_function_definition``).  If there is no
        function body, a pure specifier ("``= 0``") is checked for (see
        ``scan_pure_specifier``).
    * | ``decl_friend_function`` is called to process a friend function (member
        or nonmember).  If the function body is present, the tokens are cached
        so that they can scanned later (see ``prescan_function_definition``).
    * | ``decl_member_function_template`` is called to process a member
        function template.
    * | ``decl_typedef`` is called to process a ``typedef`` declaration.
    * | ``decl_nonstd_member_constant`` is called to process a member constant
        declaration (an extension).
    * | ``decl_static_data_member`` is called if the keyword ``static`` was
        used in a non-function declaration.
    * | ``decl_nonstatic_data_member`` is called to do processing for ordinary
        fields.
  * | At the bottom of the declarator loop, continue looking for declarators if
      the next token is a comma.
* | At the bottom of the loop, continue looking for member declarations unless
    a right brace (or end of file) is encountered.
* | Once the member declarations have been processed, further error checking is
    done, and in addition:

  * | In case the class was declared before the current definition,
      ``move_to_end_of_types_list`` is called to move the type entry for the
      class to the end of the scope types list.  (It will have been initially
      put onto the list in ``class_specifier``.)
  * | ``set_virtual_function_numbers`` is called to do the remaining processing
      for virtual functions -- in typical implementations, the virtual function
      number in a routine entry corresponds to its position in the virtual
      function table.
  * | ``check_special_member_functions`` is called to determine whether a
      compiler-generated default constructor, copy constructor, and assignment
      operator is needed and, if so, to generate the declarations.  (The bodies
      are generated if and when the function is referenced.)
  * | ``do_class_layout`` is called to allocate space for base classes and
      nonstatic data members, as well as for whatever information is needed
      (e.g., pointers) to support accessing virtual base classes and calling
      virtual functions.
  * | ``project_base_class_conversion_functions`` is called to add projection
      symbols for inherited conversion functions to the class symbol
      supplement's ``conversion_list``.
  * | ``report_virtual_function_ambiguities`` is called to issue diagnostics on
      unresolved ambiguities in virtual function declarations.
  * | ``check_abstract_class`` is called to mark a class with pure virtual
      functions as "abstract."
* | The scope stack is popped, and the closing right brace of the class
    definition is bypassed.
* | For non-nested classes ``delayed_scan_fixup_for_class`` is called to rescan
    member function default argument declarations and inline member function
    definitions.

Many of these "class definition completion" tasks are initiated from the
``complete_class_definition`` function.  This function is also called for class
types that don't appear using ordinary class definition syntax in the source
code (for example, closure types for lambda expressions; see :ref:`lambdas`).

Ideally, all properties of a class should be known once the definition has been
seen.  Unfortunately, that is not always the case.  In particular, the C++11
notion of whether a class type is a "literal type" cannot always be determined
until it is known whether it has a constexpr constructor, which in turn
sometimes requires waiting until all the field initializers have been scanned
(and for template classes that can be arbitrarily delayed).  Whether a class
type is a literal type is therefore tracked by two flags in the class symbol
supplement [#f2]_ (``known_to_be_a_literal_type`` and
``known_not_to_be_a_literal_type``), which can be determined "on demand" by a
call to ``set_literal_type_flag`` or, more generally, ``is_literal_type``.

.. _cppcli-managed-types:

C++/CLI Managed Class Definitions
---------------------------------

Managed class definitions are parsed using the same framework as standard class
definitions.  However, C++/CLI adds many new kinds of members, several of which
are implemented with new top-level mechanisms.

Several "field-like" managed member kinds rely on "context-sensitive
keywords", i.e., specific identifiers (``event``, ``initonly``,
``literal``, and ``property``) that are keywords only if the rest of the
declaration would not otherwise be valid.  The function
``check_for_cli_modifier`` looks ahead through the token stream for such
keywords, and records their presence in the ``a_decl_parse_state``
structure for the current declaration.  If the keywords ``event`` or
``property`` were found, the member declaration is handled using the
special-purpose function ``scan_cli_property_or_event_head``, after which
the front end proceeds with the next declaration.  The other two keywords
(``initonly`` and ``literal``) are consumed by the regular call of
``decl_specifiers`` (which is passed the ``a_decl_parse_state`` structure
indicating that these identifiers should be treated as keywords).

Delegate definitions appearing as member declarations also rely on a
context-sensitive keyword ``delegate``.  That case is identified by the
function ``check_for_cli_delegate_definition``, and is then handled by
``scan_and_record_cli_delegate_definition``.

Properties and events are represented as fields (``a_field``) if they are
nonstatic or as static data members (``a_variable``) if they are static.  These
representations are created by calls to ``decl_nonstatic_data_member`` or
``decl_static_data_member`` respectively, which is done by the function
``decl_property_or_event_member``.

Property and event definitions may include a brace-enclosed list of accessor
declarations (such properties/events are called *nontrivial*).  Accessors are a
special kind of member function (i.e., their ``a_routine`` entry has a
special_kind field that is not ``sfk_none``) of the parent class of the
property or event: The braces are for grouping purposes only and do not
introduce a new scope.  Accessors declarations are handled as separate member
declarations in the main loop of ``scan_class_definition``; not as part of the
call to ``scan_cli_property_or_event_head``.  However, a copy of the
``a_member_decl_info`` structure recorded for the property/event declaration
itself is kept around while scanning the accessors (accessible via the
``pe_info`` field of the current ``a_class_def_state`` structure).  This is
needed because the exact nature of the property/event may depend on the storage
class specified on the first accessor: If no storage class was specified on the
property/event declaration itself, but the first accessor is declared static,
then the property/event should be treated as static.  Hence for a nontrivial
property or events, the call to ``decl_property_or_event_member`` is delayed
until parsing of the subsequent accessor has started: At that point a lookahead
for a ``static`` specifier (using function ``static_member_next``) is performed
if needed to determine the static/nonstatic nature of the property or event.

Literal fields (i.e., fields declared with the context-sensitive keyword
``literal``) are recorded in ``decl_literal_field``: The result is an IL entry
of type ``a_constant``, not a_field (i.e., "literal field" is somewhat of a
misnomer).  ``initonly`` members, on the other hand, are just treated as
slightly special cases in ``decl_nonstatic_data_member`` and
``decl_static_data_member``.

Conversion functions and other operators can be static members of managed
classes; such members must have one more explicit parameter than their
nonstatic counterparts.  The constraints on those parameters are checked in the
function ``check_operator_function_params``.

Names
=====

Name Scope
----------

When a class is declared with a tag name, an ``sk_class_or_struct`` or
``sk_union`` symbol is created to represent the use of the name.  Since an
elaborated class specifier appearing in the source may refer to an existing
class, its name must be looked up in the symbol table.  Finding a tag name in
the current or a containing scope is handled by ``scan_tag_name``.

This is not a simple lookup, however.  An interesting case occurs when a tag
name has already been declared in an outer scope: a reference to the same name
in an inner scope may or may not be a reference to the same entity.  For
instance,

.. code:: c++

   struct S { int i; };
   void f() { struct S s; }
   void g() { struct S { /* ... */ } s; }

In this example, the ``S`` in function ``f`` refers to the ``S`` defined in the
file scope, whereas the one in function ``g`` refers to another ``S``, one
local to the function scope.  The difference is that the declaration in ``g``
is a definition and so eclipses the name from the file scope, whereas in ``f``
there no redefinition of ``S``.  For this reason ``scan_tag_name`` must take
into account whether the class is being defined in the current declaration in
deciding whether to reuse a class defined in a containing scope.  A further
complexity is revealed by another example:

.. code:: c++

   struct S { int i; };
   void f() { struct S; struct S *p; struct S { } s; }

Here the first declaration of ``S`` in function ``f``, referred to as a
"vacuous declaration" of ``S`` because it does not define a class and is not
used in the definition of any other object, has the effect of introducing an
incomplete class ``S`` into the routine scope and thereby eclipsing the file
scope ``S``.  [#f3]_ ``scan_tag_name`` also deals with this kind of case.

When no tag symbol is found, there is still the question of what scope a new
symbol is to be declared in.  This too is determined in ``scan_tag_name``; the
answer is returned to the caller by ``effective_decl_level``.  In most cases it
is simply the current scope (given in ``decl_scope_level`` [#f4]_).  However,
in C++ a non-defining initial use of a class name in a function prototype scope
or a class scope is treated as declaring that name in the innermost containing
scope that is not the scope of a function prototype or a class.  For example,

.. code:: c++

   int f(struct S *p) { return (p == 0); }
   struct S { /* ... */ } s;

Here (and contrary to the interpretation in ordinary C mode) there is only one
``S``; the second reference defines the ``struct`` to which the first is a kind
of forward reference.  Thus ``scan_tag_name`` would return the file scope as
the ``effective_decl_level`` for ``S`` in its initial declaration.  Similar
treatment is accorded declarations in classes:

.. code:: c++

   class A {
     class B;
     class C *pc;
     class D {
       class E *pe;
     };
   };

Here ``class B`` is a vacuous declaration (it anticipates a definition of a
nested ``class B`` later in the definition of ``A``), whereas ``class C`` is a
forward reference to a file scope declaration of ``C``.  Similarly, ``class E``
anticipates a file scope declaration as well, even though the file scope is not
the immediately containing scope of ``class D``.

The tag name can be a qualified name, not only in a reference (e.g., a
``friend`` declaration or the declaration of another entity using an elaborated
type specifier) but also in a definition.

* | It can be a class-qualified name for a "delayed definition" of a nested
    class.  For example,

  .. code:: c++

     class A {
       struct S;
       /* ... */
     };
     struct A::S { /* ... */ }

  | Here ``scan_tag_name`` returns a symbol locator specifying the qualified
    name; the parent class scope is then reactivated in
    ``scan_class_definition`` before the body of the class definition is
    scanned.
* | It can also be a namespace-qualified name -- most interestingly when a
    class declared in a namespace is then defined outside the namespace.  (It
    is a requirement that the scope in which the definition occurs enclose the
    scope in which original declaration occurs.) For example,

  .. code:: c++

     namespace N {
       struct S;
       /* ... */
     };
     struct N::S { /* ... */ }

  | ``scan_tag_name`` returns a symbol locator specifying the qualified name,
    and ``class_specifier`` pushes a namespace-extension scope before calling
    ``scan_class_definition``.

Tagless Classes
---------------

When classes are declared with no tag name, though obviously no symbol is
needed for lookup on a subsequent reference, one *is* needed to store
information about the class (see below).  Creating such a symbol, which is
not actually added to the symbol table, is done by
``make_unnamed_tag_symbol``.

There is a way by which a tagless class can acquire a name retroactively --
when its definition appears within a ``typedef`` declaration.  Here is an
example,

.. code:: c++

   typedef struct { /* ... */ } S;

in which case the tagless ``struct`` comes to acquire the name ``S``.  This
cannot be anticipated during the processing done by ``scan_class_definition``.
Rather, the situation is detected only after the class definition has been
scanned, in ``decl_typedef``.  In general, this means the name is assigned "for
linkage purposes", which means the ``name`` field in the type's
source-correspondence is updated.  Additionally in cfront mode,
``relink_unnamed_tag_symbol`` is called to add the symbol for the unnamed class
to the symbol table after turning it into a symbol of the specified name.

C++/CLI Accessor Names
----------------------

The names of accessor functions of properties and events (``get``, ``set``,
``add``, ``remove``, and ``raise``) are not directly visible in the class of
which they are members (although accessors are members of that class in all
other ways; e.g., they have a ``this`` handle).  Instead, referring to them
directly requires qualification with the name of the property or event.  For
example

.. code:: c++

   ref struct RS {
     int get();  // (1)
     property int p[int] {
       int get(int);  // (2) Use p::get
     }
     property int p[double] {
       int get(double);  // (3) Use p::get
     }
     property int q {
       int get(int);  // (4) Use q::get
     }
   };

The accessors marked (2) and (3) form an overload set.  The accessors marked
(1) and (4) are not part of overload sets.  (See also :ref:`cppcli-delegates`.)

The symbol for an CLI accessor is not linked under the symbol header used for
normal lookups of the accessor's name.  For example, a ``get`` accessor for a
property is not linked on the list of the symbol header normally associated
with the identifier ``get``, and hence looking up the identifier ``get`` in the
usual way will never find an accessor.  Instead, a special symbol header is
synthesized by combining the accessor name and its parent event or property;
see ``get_property_or_event_accessor_symbol_header`` for details.  (See also
:ref:`cppcli-member-funcs` for additional notes on accessor functions and their
representation, as well as :ref:`cppcli-props` for related remarks about
properties and events.)

.. _type-scope-and-symbol:

Type, Scope, and Symbol
=======================

Processing a class declaration involves forming a potentially extensive and
complex description of the class.  There are three principal data structures
that serve as repositories for this information: a symbol entry, a type entry,
and an IL scope entry.  The information that must be passed on to the back end
(including the IL lowering phase, if applicable) is stored in the type and
scope entries; information that is only of use in front end processing (e.g.,
for error detection) is recorded in the symbol.

Detailed descriptions of these data structures are provided in the files
``il_def.h`` and ``symbol_tbl.h``; introductions painted with a broader brush
may be found in the Intermediate Language and Symbol Table chapters.

One thing that might be a bit confusing is that both the symbols and type
entries for classes have supplements, entities of type
``a_class_symbol_supplement`` and ``a_class_type_supplement``, respectively.

Moving among the symbol, type, and scope entries that represent a class is
fairly straightforward:

* | The ``assoc_type`` field of the IL scope entries points to the associated
    class type.
* | The ``assoc_scope`` field of the class type supplement points to the
    corresponding IL scope entry.
* | In the type entry the ``source_corresp.assoc_info`` field is a pointer to
    the class's symbol entry.  [#f5]_
* | The symbol points to the type entry by means of the ``type`` field.

To make it a bit easier to get from source language categories to the data
structures that represent them, here (from a slightly different slant) is yet
another description of how the symbol, type, and scope divide up the task of
summarizing a class declaration:

* | Nonstatic data members ("fields") are recorded on a linked list of
    ``a_field`` that is accessed from the type entry.
* | Static data members are represented by ``a_variable`` entries; a list of
    them appears on the scope entry.
* | Member functions appear on a linked list of ``a_routine`` entries which is
    also accessed from the scope entry.  Static and nonstatic member functions
    are intermingled on this list; they can be distinguished by the presence or
    absence of an implicit ``this`` parameter type in the associated routine
    type.
* | Member function templates are represented by ``sk_function_template``
    symbols.  (They also appear in the IL, but only very limited information is
    available there: see the linked list of ``a_template`` pointed to from the
    scope entry for the class.) Instances of member function templates, like
    ordinary member functions, appear on the ``routines`` list of the class
    scope entry.
* | As far as the IL is concerned, constructors, destructors, overloaded
    operator routines, and user-defined conversion routines are just like any
    other member function.  They can be identified by the
    ``special_function_kind`` value in the routine entry (e.g., for generating
    their names).  To facilitate front-end processing, however, the symbol's
    class supplement contains pointers to the symbols for the constructor(s),
    the destructor, and the assignment operator routine(s).
* | Member types (including type entries for nested classes) and member
    constants also appear on lists hanging off the scope entry.
* | Member class templates are represented by ``sk_class_template`` symbols.
    (They also appear in the IL, but only very limited information is available
    there: see the linked list of ``a_template`` pointed to from the scope
    entry for the class.) Instances of member class templates, like ordinary
    nested classes, appear on the ``types`` list of the class scope entry.
* | Friendship is represented by a reference to the befriending class in the
    routine entry for the friend function (or in the type entry when friendship
    is conferred on an entire class).
* | Derivation is represented by a list of ``a_base_class`` entries that is
    pointed to from the class type supplement of the derived class.  All base
    classes (direct, indirect, virtual) appear on this list.  The specific ways
    the class is derived are given by the ``a_base_class_derivation`` entry
    associated with each base class; and there may be more than one such entry
    for a given base class if it is virtual.
* | Inherited names are represented by ``sk_projection`` symbols.  The
    projection symbol identifies both the fundamental symbol and the base class
    entry with which that instance of the symbol is associated.
* | Names brought into a derived class from a base class by a
    *using-declaration* (or an access declaration) appear in the symbol table
    as projection symbols for which ``is_using_decl`` is TRUE.  Such
    declarations are also recorded in the IL by entries of type
    ``a_using_decl``, pointed to from the scope entry.
* | Accessibility is recorded in IL entries (for front-end use and symbolic
    debug purposes only), in projection symbols (which can end up marked
    "inaccessible"), and in base class derivation entries.  But since
    accessibility is generally dynamic, it must usually be computed (e.g., see
    ``access_to_end_of_path`` and ``have_access_to_symbol`` in
    ``symbol_tbl.c``).

Base Classes
============

Once the tag symbol and type entry have been created for a class definition,
``scan_class_definition`` calls ``scan_base_specifier_list`` to scan any base
classes that may have been specified.  It goes through the comma-separated list
of base classes one by one, creating for each (and for each of the indirect
base classes derived through it) an entry of type ``a_base_class`` that is
added to the ``base_classes`` list of the class type supplement.  When all base
classes have been added, ``wrapup_base_classes`` is called to establish certain
properties (like the preferred derivation path of virtual bases) that cannot in
general be determined by looking at a single derivation.

Base Class List
---------------

The list of base classes associated with any derived class contains a base
class entry for each of its base classes -- not just for those from which it is
directly derived.  The order of the list is significant: depth-first
left-to-right, following the source code order of declarations.  Base classes
contain a ``direct`` flag to distinguish direct from indirect base classes;
there is also an ``is_virtual`` flag.  Consider this example:

.. code:: c++

   class V { /* ... */ };
   class A : virtual V { /* ... */ };
   class B : virtual V { /* ... */ };
   class C : public B { /* ... */ };
   class D : public C, public A { /* ... */ };

Here the base classes list for class ``D`` will contain the following entries;
note that the virtual base class appears only once in the list:

* | Indirect virtual base class ``V``
* | Indirect base class ``B``
* | Direct base class ``C``
* | Direct base class ``A``

Base class entries are not shared between classes because they contain
information uniquely identifying the relation of a base class to its derived
class.  Thus, in the previous example, class ``C`` also has its own base
classes list, namely:

* | Indirect virtual base class ``V``
* | Direct base class ``B``

Despite some overlap of information, new copies of the base class entries of
``C`` are made for the base classes list of ``D``.

In ``scan_base_specifier_list``, for each base specifier in the source the
optional access-specifier and optional ``virtual`` keyword are scanned, the
base class name is scanned, and error checking is performed.  If the same base
class name appears twice in the base specifier list, an error is issued.
However, if the name of a direct base class is identical to that of an indirect
base class already on the list, no error is reported at this time; rather, both
base classes are marked as ambiguous.  This is an example of such a case:

.. code:: c++

   class A { /* ... */ };
   class B : public A { /* ... */ };
   class C : public B, public A { /* ... */ };

The base classes list for ``C`` will contain:

* | Indirect base class ``A`` (marked ambiguous)
* | Direct base class ``B``
* | Direct base class ``A`` (marked ambiguous)

Only later, if, say, a qualified name using ``A`` appeared within a member
function of ``C``, would the base class ambiguity be reported.

Creating the base classes list is done by ``scan_base_specifier_list``, which
adds direct base classes, and ``add_indirect_base_classes``, which in recording
indirect base classes calls itself recursively as required.  Both routines call
``fixup_virtual_base_class`` when a virtual base class is encountered more than
once among the direct and indirect base classes.

Derivation Path
---------------

Each base class entry contains a ``derivation`` field, which points to one
or more entries of type ``a_base_class_derivation``.  (Nonvirtual base
classes will have only one such entry, but virtual bases may have more.)
Each base class derivation entry points to a linked list of one or more
entities of type ``a_derivation_step``.  Such a list is referred to as a
"derivation path" because it records the derivation of the derived class
from the base class: it is the path *from* a derived class back up its base
class tree *to* the required base class.  Here is a simple example:

.. code:: c++

   class A { /* ... */ };
   class B : public A { /* ... */ };
   class C : public B { /* ... */ };

The derivation path from derived class ``C`` to direct base class ``B`` is
=>B, whereas the path to indirect base class ``A`` is =>B=>A.  (The notation =>
can be thought of as a cast from a derived to a direct base class.)

The first step on a derivation path is either a direct base class, as in the
examples above, or a virtual base class.  In the latter case, the paths to the
virtual base class (there may be more than one) are elided and may be found by
examining its base class derivation entries.  For example:

.. code:: c++

   class X { /* ... */ };
   class A : public X { /* ... */ };
   class B : virtual public A { /* ... */ };
   class C : virtual public A { /* ... */ };
   class D : private B, public C { /* ... */ };

Here the derivation path for ``X`` in ``D`` is represented as =>A=>X, whereas
``A`` itself has two derivation paths, =>B=>A and =>C=>A, which are in effect
implicit in the derivation for ``X``.

Base class ambiguity can be seen as a case in which there are two distinct
paths to nonvirtual base class entries that refer to the same class.  Returning
to a previous example,

.. code:: c++

   class A { /* ... */ };
   class B : public A { /* ... */ };
   class C : public B, public A { /* ... */ };

there are two base class entries for ``A`` in class ``C``, with derivation
paths of =>B=>A and =>A.

``add_indirect_base_class`` takes an existing base class entry and creates a
new one based on it, but to do this it needs a new derivation path as well.
``copy_and_extend_path`` creates a new path based on the derivation path of the
original base class entry.

For cases in which it is necessary to compare two derivation paths,
``congruent_paths`` returns TRUE if two paths refer, step for step, to the same
classes.

Virtual Base Class
------------------

When two or more base classes of the same type are specified ``virtual``, they
share the same base class entry.  Its position in the base classes list will be
that of first appearance in the depth-first left-to-right traversal of the base
classes.  However, each appearance in the derivation graph is recorded with a
separate derivation entry, and each derivation entry gives a unique derivation
path.

``set_preferred_base_class_derivation`` is called to identify the "preferred"
path of a virtual base class -- tantamount to the sequence of casts that
affords the greatest "normal" accessibility (i.e., without special treatment
for casts in the context of member or friend functions).  For example,

.. code:: c++

   class A { /* ... */ };
   class B : virtual public A { /* ... */ };
   class C : virtual public A { /* ... */ };
   class D : private B, public C { /* ... */ };

Class ``D`` is derived from virtual base class ``A`` via two paths, BA and CA.
Therefore ``A``\ 's base class entry points to a list of two base class
derivation entries, one for each path.  Since the path through ``C`` gives
greater accessibility, the base class derivation entry that points to CA is the
one marked "preferred".  (When several entries afford equally good access, a
direct base class is preferred over an indirect, and an indirect base class
with no virtual base classes in its derivation is preferred over one that has
virtual base classes in its derivation.)

Virtual Functions
-----------------

A final task performed by ``scan_base_specifier_list`` and its subroutines is
to accumulate virtual function information for its base classes.

When a function in a derived class "overrides" a virtual function from one of
its base classes, that information is recorded for later use: [#f6]_ A list of
entries of type ``an_overriding_virtual_function`` is pointed to from the base
class entry to identify the virtual functions that are members of that class
and that are overridden in some other class derived from it.  This information
must be propagated as base classes are copied.  Here is an example to make this
a bit clearer:

.. code:: c++

   class A { virtual void f(); };
   class B : public A { void f(); };
   class C : public B { };

``A::f()`` is a virtual function of the base class ``A`` that is overridden
by ``B::f()`` in the derived class ``B``.  The
``overriding_virtual_functions`` field of the base class entry for ``A`` in
``B`` points to an entry that identifies ``A::f()`` as the *primary
function* and ``B::f()`` as the *overriding function*.  When ``C`` is
derived from ``B``, this information is passed along.  At the time the base
class entry for ``A`` is created in ``C``, the virtual function override
information is copied from the base class entry for ``A`` in ``B``, with an
additional piece of information -- a pointer to the base class entry for
``B`` in ``C``.  [#f7]_

The virtual function override list is sorted on the basis of the virtual
function number in the routine entry of the primary function.  It can be that
more than one overriding virtual function entry for a given primary function
ends up on the overriding virtual function list -- and it needn't be illegal.
Consider the following example:

.. code:: c++

   class A { virtual void f(); };
   class B : virtual public A { void f(); };
   class C : virtual public B { void f(); };
   class D : public B, public C { };

Within both ``B`` and ``C`` there are an overriding virtual function entries
for base class ``A`` specifying that primary function ``A::f()`` is overridden,
in the one instance by ``B::f()`` and in the other by ``C::f()``.  Within ``D``
there is also a base class entry for ``A``, and both the overriding by
``B::f()`` and the overriding by ``C::f()`` are recorded in its list; both
overriding function entries will refer to the same primary function,
``A::f()``.  As written above, this example is illegal.  But the diagnostic
cannot be issued until the definition of ``D`` is complete (see
``report_virtual_function_ambiguities``), since a declaration ``D::f()`` would
override both ``B::f()`` and ``C::f()``, making the ambiguity moot and the
program legal.

``copy_virtual_function_override_list`` copies the overriding virtual function
for a base class from one class to another derived from it, calling
``insert_in_virtual_function_override_list`` to place the copy in the proper
sorted location in the new list.

C++/CLI Derivations
-------------------

C++/CLI managed classes can derive from base classes in two different ways:
They can "inherit" from one ref class and they can "implement" any number of
interface classes.  If the definition of a managed class (other than
``System::Object``) does not specify a base class explicitly, it inherits
``System::ValueType`` if it is a value class, or it derives virtually from
``System::Object`` otherwise (hence, all managed classes ultimately derive from
a virtual base class ``System::Object``).  See ``add_implicit_cli_bases``.

"Implementing an interface" is modeled as virtual derivation.  Inheriting from
a ref class other than ``System::Object`` corresponds to regular (i.e.,
nonvirtual) derivation.

Some ref classes must implicitly directly derive from (i.e., "implement") the
``System::IDisposable`` interface according to a set of rules known as the "CLI
dispose pattern" (see ``implement_dispose_pattern_if_needed``).  However, this
cannot be determined until after the complete class definition has been seen:
This is therefore a unique situation where a (virtual) base class is added
after members have been seen.  When this is done, ``wrapup_base_classes`` must
be called a second time.

Some generated managed classes implicitly inherit specific ref classes (by
calling ``add_cli_system_base_class``): delegate classes inherit
``System::MulticastDelegate`` and boxed enum types inherit ``System::Enum``.

Generic parameters are modeled using proxy (managed) class types called
"constraint types".  A type constraint specifying a type C on a parameter
modeled by a constraint type P is modeled by adding C as a base class of P.
Note that Microsoft compilers permit "sealed" classes [#f8]_ to be specified as
type constraints, which means that in this context a sealed class can be
derived from (and in that case, the deriving constraint type is marked as
sealed also).

Fields
======

Declarations
------------

``scan_nonstatic_data_member`` is called from ``class_member_declaration`` to
process the declarations of what in ordinary C are called "fields" and in C++
are "nonstatic data members".  It handles parsing of bit field widths and
C++11-style initializers (see below), and calls ``decl_nonstatic_data_member``
to create the actual IL entry (type ``a_field``) and associated symbol (if any)
as well as to do some error checking and set various flags.  [#f9]_

Field symbols point to their corresponding IL entry and also to a field symbol
supplement that carries some additional (mostly C++-specific) information.

Processing is straightforward for ordinary fields.  There are several special
cases that warrant a little more explanation, however.

* | For bit-fields ``scan_fs_constant_expression`` is called to scan the size
    specified after the colon.  It scans the constant value.  This constant is
    then used by decl_nonstatic_data_member to perform error checking, and, in
    a few unusual cases, to modify the field's type to accommodate the
    bit-field (e.g., when a signed integer kind was specified but the bit-field
    is too small for a sign bit).
* | For unnamed fields a field entry is created and added to the IL tree (the
    field entry is needed when ``set_field_size_and_offset`` is called), and a
    symbol is created for it (see ``unnamed_field_symbol`` in
    ``symbol_tbl.c``).  The space allocated by the unnamed field is represented
    as a gap left in the layout of the class.
* | Anonymous union "parent fields" have no names, but they do have an
    associated symbol and a field entry in the IL.  In addition,
    ``check_anonymous_union_symbols`` is called both to do error checking and
    to promote the symbols for the fields of the anonymous union into the name
    scope of the class of which the anonymous union is a member field.  This
    produces an inconsistency between the IL representation, which is that of
    any nested union, and the symbol table representation, where the names of
    the fields of the anonymous union are treated as though they were declared
    in the containing class -- for example, the parent class of the promoted
    ``sk_field`` symbol is the containing class, but the IL field entries to
    which it points continues to be represented as a member of the nested
    anonymous union.

In addition to actually doing error checking, ``decl_nonstatic_data_member``
also sets some flags in the symbol to facilitate error checking and other
processing at a later time.  For instance, if the field's type is a reference
type, ``any_ref_member`` is set in the class symbol supplement; the setting can
then be checked to determine whether the compiler can generate a default
constructor.

Offset and Alignment
--------------------

The front end is configurable to use either of two models for assigning offsets
to fields.  Either a class is laid out so that the fields are allocated exactly
in the order in which they are declared, or else they are grouped by access,
with first the ``public`` fields allocated in order of declaration, then the
``protected`` and finally the ``private`` fields.

Processing for both models is done by ``set_offsets_for_fields``, called from
``do_class_layout`` in ``layout.c``.  For each field it calls
``set_field_size_and_offset``, which takes as input offset values left over
from the previous allocation and uses them to allocate the current field.  The
maximum alignment required for the structure as a whole is also tracked.  The
computations are done in ``increment_field_offsets``, which does overflow
checking on all calculations.

By default, fields are assigned offsets in declaration order; the other
model is activated by setting
``TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE`` to ``0``.  *However, at
this time only the default allocation model is supported by the IL lowering
pass.*

.. _field-inits:

Field Initializers
------------------

C++11 allows a field initializer (also known as a DMI -- default member
initializer -- or, previously, an NSDMI -- non-static data member
initializer) to be specified on a field declaration.  For example:

.. code:: c++

   struct S {
     int i = init_val;
     static int init_val;
   };

The initializer must be parsed in the context of the completed class (which,
e.g., means a field initializer can make use of a member that is declared at a
point that is lexically after the initializer).  To achieve this,
``scan_nonstatic_data_member`` caches the initializer for later parsing.  In
the case of nontemplate classes, this later parsing is handled by recording a
fixup entry (by calling ``record_inclass_initializer_fixup``) which is later
processed by a call to ``inclass_initializer_fixup`` (when the class is
completed), and that in turn calls ``field_initializer`` (in ``decl_inits.c``)
to actually parse and record the initializer.  For template classes, the
initializers are not parsed until they're needed.  If a field initializer of a
template class must be accessed by the front end, it must call
``instantiate_field_initializer_if_needed`` prior to examining the IL for that
initializer (recorded in ``a_field::initializer``).  In the template case also
``field_initializer`` is the routine that performs the actual parsing of the
initializer.

Field initializers are subject to various error checks.  One of the more subtle
checks in this context is for circular dependencies: The initializer for a
field may depend on that field's value in various indirect ways.  The front end
will diagnose this if the circularity affects compile-time evaluation
(particularly, when dealing with constexpr constructors).

Static Data Members
===================

``decl_static_data_member`` is called to process declarations of static data
members.  It creates a variable entry (typically with ``sc_extern`` storage
class -- it will later be changed to ``sc_unspecified`` if it is defined in the
current translation unit) and appends it to the ``variables`` list of the IL
scope entry for the current class.  It also creates a symbol entry of kind
``sk_static_data_member`` and enters it into the symbol table.

When a static data member of integral type is declared as ``const``-qualified
and an initializer appears in the in-class declaration, it is usable as a
member constant.  The initializer expression is scanned by
``scan_member_constant_initializer_expression``, and the variable is flagged
with ``is_member_constant`` set to TRUE.

Definitions of static data members, with optional initializer, are handled as
part of normal declaration processing, where ``define_static_data_member`` is
called from ``function_definition``.  Aside from setting a few flags, little
needs to be done except some error checking.

C++/CLI Static Data Members
---------------------------

Static data members of C++/CLI managed classes can have an initializer when
they're first declared in the class, and that declaration is then considered to
be a definition.  Unlike standard static data members, there are no type
restrictions for this in-class initialization to be permitted and any
initializer that would be allowed in namespace scope can be specified.

Furthermore, such initializers are by default not processed until the complete
class definition has been seen [#f10]_.  This parallels the standard treatment
of in-class function definitions and in-class default arguments.  However, if
the value of the static data member is needed to evaluate a
constant-expression, the initializer is processed at that point (i.e., early;
see ``ensure_inclass_static_member_constant_initializer_is_scanned``).

Member Functions
================

``decl_member_function`` is called to complete processing a member function
declaration, but the work has already been started by ``function_declarator``,
which needs to recognize member function declarations for two reasons:

* | Default arguments in parameter declarations are not passed to
    expression-scanning routines immediately; rather, the tokens are cached and
    scanned only once the entire class declaration has been completed.
* | After the parameters have been scanned, an optional ``const`` or
    ``volatile`` qualifier may be present on nonstatic member functions.  And
    whether the qualifier is there or not, the type of the implicit ``this``
    parameter is determined in ``function_declarator`` and the routine type
    supplement for the member function's routine type entry is updated
    accordingly.

Consequently, even before ``decl_member_function`` is called, quite a bit of
processing has been done on member functions.  What remains is to create a
symbol entry and a routine entry for the member function, add the symbol to the
symbol table, and append the routine entry to the ``routines`` list of the IL
scope entry for the current class.  Some special processing is required for
overloaded functions, for virtual functions, for constructors and destructors,
for user-defined conversions, and for operator functions.  The field
``special_kind`` of ``a_routine`` entries identifies special member functions
(like constructors, destructors, operators, C++/CLI accessors, etc.); it is set
to ``sfk_none`` for ordinary member functions (and non-member functions).

Static *vs.* Nonstatic
----------------------

As noted above, a nonstatic member function receives special treatment in
``function_declarator``: the ``implicit_this_param_type`` field of the routine
type supplement is updated with a pointer to the (optionally qualified) class
of which the function is a member.  In a static member function this pointer
remains NULL.  This is the only way static and nonstatic member functions are
distinguished in the IL.  ``routine_type_is_nonstatic_member_function`` is a
macro that is available to test the field.

Overloading
-----------

Function overloading is a matter for name lookup only -- that a function is
overloaded is not explicitly represented in the IL.  The general technique for
supporting function name overloading with ``sk_overloaded_function`` symbols is
discussed in detail in the chapter on the Symbol Table.

Entering a new member function symbol in a way that takes overloading into
account is handled by ``symbol_for_member_function``.  One of the subroutines
it calls is ``member_function_redecl_sym``, which checks for type
compatibility, recognizing when the type of the implicit ``this`` parameter
should be ignored and when it must be taken into account.

In addition to ``sk_member_function`` symbols, the overload set may include
``sk_projection`` symbols representing ``using``-declarations and
``sk_function_template`` symbols representing member function templates.

Virtual Functions
-----------------

A member function may be explicitly declared ``virtual``, or it may be a
virtual function implicitly, because of having the same name and type as a
virtual function from a base class.  In either case, for every member function
it is necessary to go through all the base classes and look for functions with
the same name and type.  Then, if the original function is virtual for either
reason, not only must it be so marked, but it is also necessary to record the
fact that a virtual function from a base class is overridden by a function from
a derived class.  This, as already described in another context, is done with
entries of type ``an_overriding_virtual_function``.

``decl_member_function`` calls ``check_for_virtual_override`` to accomplish all
this.  The latter does the search through the base classes for overridden
virtual functions, and when it finds one it calls
``record_virtual_function_override`` to add an entry to the
``overriding_virtual_functions`` list in the base class.  When an entry that
refers to the same primary function is already there (as a result of an
override by an inherited function), that entry is updated to record the
override by the current class's member.  When more than one such entry is on
the list, the extras are removed, since the ambiguity is resolved by the
current declaration.

After all declarations of the current class have been processed,
``scan_class_definition`` calls ``set_virtual_function_numbers`` to assign
sequential numbers to nonoverriding virtual functions declared in the current
class.  (Overriding virtual functions will have already been assigned the
number of the associated base class function.)

Then it calls ``report_virtual_function_ambiguities``, which makes another pass
over the base classes to look for ambiguous virtual function overriding -- it
issues errors when such ambiguities, caused by merging overriding virtual
function lists from several base classes, are not resolved by a member function
declaration in the current class.

Default Arguments
-----------------

Default arguments specified for parameter declarations in member function
declarations require special handling, because the expressions must not be
scanned until all the member declarations in the class have been scanned.
Parameter declarations are scanned in ``function_declaration``.  When a default
argument is encountered in a member function declaration that is part of a
class definition, ``prescan_default_arg_expr`` is called to cache the tokens
that make up the expression; the cache resides in an entry of type
``a_def_arg_expr_fixup``, which is kept on a list pointed to by an entry of
type ``a_routine_fixup`` that is accessed from the class symbol supplement.
When the class's member declarations are complete, that list is revisited and
the default argument expression is rescanned (see
``delayed_scan_of_default_arg_expr``, which is called from
``delayed_scan_fixup_for_class``).

Inline Definitions
------------------

When a member function body appears within a class definition, it is handled
somewhat like default argument expressions: ``prescan_function_definition`` is
called to cache the tokens that comprise the function body (the cache resides
in an entry of type ``a_routine_fixup``), and when the class definition is
complete, ``delayed_scan_fixup_for_class`` calls ``scan_function_body`` (in
``func_def.c``) to process the function body retroactively.  (For constructors
that are defined inline, the constructor initializer list is included among the
cached tokens, and ``ctor_initializer`` (in ``decl_inits.c``) is later called
to scan it.)

A somewhat special case occurs when inline definitions and default argument
expressions appear within a nested class: the language definition requires that
a reference to a name in the enclosing class be found even if that name is
declared after the nested class definition is complete.  For instance,

.. code:: c++

   struct Outer {
     struct Inner {
       static int f() { return i; }
     };
     static int i;
   };

Since a call to ``Inner::f()`` should return the current value of ``Outer::i``,
the body of ``Inner::f()`` cannot be processed until all the members of
``Outer`` have been seen.  Therefore the delayed scan fixup list for a nested
class is not rescanned until its outermost containing class has been completely
scanned.  Only then does ``scan_class_definition`` call
``delayed_scan_fixup_for_class``, which in turn calls itself recursively for
nested classes before processing the tokens cached for the members of the class
passed to it.

User-Defined Conversions
------------------------

When ``decl_member_function`` is called for a user-defined conversion, it
creates an entry of type ``a_conversion_list_entry`` to record the existence of
the conversion routine in the class symbol supplement for the current class.
The entry simply points to the symbol for the user-defined conversion; all such
routines for a given class are accumulated on a linked list.

Since conversion functions are functions that convert an object of the current
class to a specified result type, the list that is created can serve as a list,
for a given source type, of all the target types for which a user-defined
conversion exists.  This kind of list can be useful in selecting the
appropriate implicit cast during expression processing.

User-defined conversion functions are inherited, so after all member
declarations have been seen, the list is augmented by projection symbols for
inherited conversion functions that result in types that are not already
present in the list (see ``project_base_class_conversion_functions`` which goes
through the conversion lists for each of the direct base classes of the current
class looking for unrepresented result types).

Compiler-Generated Functions
----------------------------

A default constructor or a copy constructor is created by the compiler when it
is required in a class but was not explicitly declared by the user.  Likewise a
compiler-generated destructor may be needed, or an assignment operator which
effectively copies an object of a given class onto another object of the same
class.  Even if one of these functions is needed, only a declaration is
generated at first, not a function body.  The body is generated only when there
is an actual invocation of the function.  The functions will be declared as
extern inline member functions, just as if they had been explicitly defined in
the class definition.

After all member declarations have been scanned,
``check_special_member_functions`` is called.  For each compiler-generated
function determined to be needed, it calls ``generate_special_function``, which
creates a routine type entry and calls ``decl_member_function`` to create the
symbol and the routine entry.

``reference_to_implicitly_invoked_function`` is called when a
compiler-generated function is implicitly invoked.  If the function has no
body, ``define_special_member_function`` (in ``func_def.c``) is called, which
in turn calls ``make_default_constructor_body``,
``make_default_destructor_body``, or ``make_default_assignment_body``.  The
compiler-generated assignment function is created only when a simple ``struct``
copy is insufficient, i.e., when memberwise copying must be done explicitly.

Trivial default constructors present a special problem.  They are "trivial"
because calling them has no actual effect, so an obvious optimization is to
eliminate them.  When the symbol supplement for a class has a NULL
``constructor`` pointer, that means the implicitly declared default constructor
for the class (always present in principle) is trivial and can generally be
ignored.  There is an exception, however, when the definition of a trivial
default constructor would be ill-formed: then, even though no call is put out
in the IL, a diagnostic must still be issued.  Here's an example:

.. code:: c++

   class A {
     const int i;
   };
   A var;

Since class ``A`` is not a "POD-struct", ``var`` is default-initialized, which
means the default constructor ``A::A()`` is supposed to be called, and calling
an implicitly-declared constructor normally triggers its definition.  The call
can be optimized away since it has no effect, but the definition would be
ill-formed (because ``A::i`` is ``const``-qualified).  So to preserve the
behavior *as if* ``A::A()`` were called, its declaration is saved in the symbol
(see ``trivial_default_constructor`` in the class symbol supplement) and its
definition is generated at the point of implicit reference by a call to
``reference_to_trivial_default_constructor`` in ``symbol_ref.c``.

.. _cppcli-member-funcs:

C++/CLI Member Functions
------------------------

C++/CLI adds several variations for member function declarations in managed
classes.

Syntactically, a static constructor is like a default constructor (i.e., with
no parameters) declared with the storage class ``static``.  The two can
coexist, but they don't create an overload set.  In expression contexts,
default construction always refers to the nonstatic constructor kind.  In
declaration contexts, the static constructor is only found if the declaration
includes ``static`` (which is communicated to the lookup functions with the
flag ``IDL_IS_STATIC_DECL``).  Static constructors are the only members that
can be defined outside their parent class with an explicit storage class
specifier (and such definitions must include that ``static`` specifier).

Managed classes can contain finalizers: Their handling is completely parallel
to that of destructors.  The syntax uses "``!X``" instead of "``~X``".

User-declared accessor functions for properties and events are explained in
:ref:`cppcli-managed-types`.  For trivial properties and events (which don't
include user-declared accessors), accessors are generated by calls to
``generate_trivial_accessor``.  Whether generated or user-declared, the
``a_routine`` for an accessor function points to the
``a_property_or_event_descr`` entry associated with the corresponding event or
property.

The member function modifiers ``sealed``, ``abstract``, and ``override`` are
accepted in both ordinary Microsoft mode and in C++/CLI mode.  They can appear
after the optional const and/or volatile at the end of a nonstatic member
function declarator (see ``scan_microsoft_function_modifiers``), but in C++/CLI
``override`` is not optional for derived class members that override a member
in a base ref class.  The modifier ``new`` is also accepted at this grammatical
location to indicated that a function does not implicitly override a virtual
base member with the same name and type.

A virtual member function can also specify specific base members it overrides
using a specifier like "``= B::m, C::n``" where ``B`` and ``C`` are the base
classes or interfaces whose respective members ``m`` and ``n`` are
overridden/implemented.  This is called "named overriding" (in contrast to
"implicit overriding").

Named overriding and the requirements of the ``override`` and ``new`` modifiers
are mostly handled in ``check_for_virtual_override`` and its helper functions.
These features add considerable complexity to that function.

Member Function Templates
=========================

``scan_class_definition`` calls ``template_declaration_or_directive`` (in
``templates.c``) when a member declaration begins with the keyword
``template``.  When the declaration designates a function template, this
results in a call to ``class_member_template_declaration``, which in turn calls
``class_member_declaration``.  Once processing by ``decl_specifiers`` and
``declarator`` has been done, ``decl_member_function_template`` is called to
create the ``sk_function_template`` symbol.  A routine entry is also created,
but (as with nonmember template functions) it is not added to the IL.  Rather,
it is attached to the template symbol supplement and is for front-end use only.

Member Constant Extension
=========================

As an extension to C++ the EDG front end allows "member constant" declarations.
[#f11]_ Member constants may be of any scalar type.  For integers such a
declaration is similar to declaring a constant name in an ``enum`` declaration.
For example, instead of this:

.. code:: c++

   class A {
     enum { MAX = 100 };
     int array[MAX];
   };

the program may declare and use a member constant:

.. code:: c++

   class A {
     const int MAX = 100;
     int array[MAX];
   };

Qualified-name references to member constants (e.g., to ``A::MAX``) are also
permitted, subject to the same access and ambiguity constraints imposed on any
class member.

``class_member_declaration`` calls ``decl_nonstd_member_constant`` to process
such a declaration.  It adds to the IL an entry of type ``a_constant`` (it is
appended to the ``constants`` list of the IL scope associated with the current
class), and it enters an ``sk_constant`` symbol that points to it.  Note that a
member constant is not equivalent to an initialized ``const`` static data
member, which is represented as a variable in the IL.

Friend Declarations
===================

A ``friend`` declaration involves (1) a class which gives ``friend`` access to
nonmembers and (2) the routine or class to which that access is given; and so
one may refer to the former as the *befriending* class and the latter as the
*befriended* routine or class.

The class type supplement of the befriending class contains pointers
(``friend_routines`` and ``friend_classes``) to linked lists of entries
identifying the routines and classes to which friendship is given.  This
corresponds to the way in which a friend declaration actually appears in the
source.

In addition, the befriending-befriended relationship is represented the other
way around as well, with the befriending class also recorded in the befriended
routine or class.  Specifically, routine entries have a pointer to a list of
entries that point to classes; the linked list identifies the befriending
classes, those that afford ``friend`` access to the specified routine.  A
similar ``befriending_classes`` field can be found in the type supplement for a
befriended class.

``decl_friend_function`` is called by ``class_member_declaration`` to process a
``friend`` function declaration.  For a member function
``member_function_redecl_sym`` is called to get the symbol; otherwise,
``decl_routine`` in ``decls.c`` is called (where, if this is the first
declaration of the function, the symbol will be created in the appropriate
non-class scope).  Once the symbol is retrieved, the current class is recorded
as a befriending class in the routine entry.

``decl_friend_class`` is called to process a ``friend`` class declaration.
This is just a matter of recording the current class as a befriending class in
the type supplement of the befriended class.

A class- or namespace-qualified ``friend`` declaration must refer to a
previously declared entity, but it may be that the first declaration of an
entity is an *un*\ qualified ``friend`` declaration.  The lookup for an
unqualified ``friend`` declaration begins in the innermost enclosing
nonclass scope and continues to (but no farther than) the innermost
enclosing namespace scope, and if it is not found a declaration is
"injected" into the innermost enclosing nonclass scope.  [#f12]_ For
instance:

.. code:: c++

   namespace N {
     class S {
       friend void f();
       /* ... */
     };
     void f() { /* ... */ }   // Same "f" as declared a friend by N::S
   }

This approach is taken even when the specified name is visible in a scope
enclosing the innermost namespace scope -- in the above example, ``f`` would be
injected into namespace ``N`` even if a ``::f`` were visible.

Using Declarations and Access Declarations
==========================================

A ``using``-declaration inside a class definition is processed by
``member_using_declaration``, which is also called when
``scan_class_definition`` encounters a declaration that is a class-qualified
name followed by a semicolon, interpreted to be an old-style access declaration
of an inherited member.  ``member_using_declaration`` performs error checking
and causes a projection symbol to be created to reserve the name in the current
class.  The flag ``is_using_decl`` is set in the projection symbol, since this
is not an ordinary kind of projection and is subject to somewhat different
rules for computing accessibility; [#f13]_ it can also be added to an overload
set if it refers to an inherited member function.  In addition, the declaration
is recorded in the IL by adding an entry of type ``a_using_decl`` to the scope
associated with this class, in case a symbolic debugger would need the
information (see ``make_using_decl`` in\ ``decls.c``).

.. _cppcli-delegates:

Delegates (C++/CLI mode)
========================

A C++/CLI delegate is a managed function object (i.e., a "callable" object)
that encapsulates an invocation list.  A delegate type is introduced by a
delegate definition, which looks like a typedef for a function type where the
keyword ``typedef`` is replaced by a context-sensitive keyword ``delegate``.
The front end represents a delegate type as a mostly ordinary ref class type
synthesized from the signature provided in the definition.  For example, a
definition like

.. code:: c++

   delegate void D(System::Object^);

causes the front end to synthesize (in ``create_cli_delegate_definition``)
a ref class called ``D``, implementing the interface
``System::MulticastDelegate`` with (among others) a member function called
``Invoke`` with the signature of the delegate definition (to which "calls"
of delegate objects are dispatched), a constructor, and static member
operators ``+`` and ``-`` (to add or remove targets from the encapsulated
invocation list).

.. _cppcli-props:

Properties and Events (Microsoft and C++/CLI modes)
===================================================

Microsoft C++ includes a notion of "property" members for (otherwise
standard) class types.  Syntactically, this is a field declared with an
attribute of the form ``__declspec(property(get=``\ *func_name1*\ ``,
put=``\ *func_name2*\ ``))``.  The result is not really a field: No storage
is allocated for it, but every "read" operation on the pseudo-field is
mapped to a call of *func_name1* and every "write" operation is mapped to a
call of *func_name2* (either operation name may be omitted from the
attribute to create a read-only or write-only property).  The IL
representation of this kind of property members uses ``a_field`` entries,
which point to entries of type ``a_property_or_event_descr``, and those
entries in turn record the name of the specified get and/or put functions.
*func_name1* and *func_name2* are looked up at every point that requires
their use, which may not always yield consistent results.  Therefore, the
rewriting of property-field accesses is done right away in the front end
proper (and not, for example, in a later lowering pass since the symbol
table might have changed by then).

C++/CLI provides a similar notion of properties for managed class types, but
instead of indicating the "accessor functions" (named ``get`` and ``set`` in
this case) by name in an attribute, they are part of the property definition
syntax.  For example:

.. code:: c++

   ref struct RS {
     property int p { int get(); void set(int); }
   };

The IL representation is similar to that of the first kind of property (i.e.,
the non-C++/CLI variant described above), but since the accessor functions are
independent from the point of use in this case, the corresponding ``a_routine``
entries are pointed to directly from the ``a_property_or_event_descr`` entry
and vice versa.  Also, since C++/CLI properties can be static or nonstatic,
their representation can be entries of type ``a_field`` and ``a_variable`` (the
latter corresponding to a pseudo-static-data-member).

C++/CLI properties can be "indexed" to model a sort of "array field" (but
unlike regular fields, indexed properties can be truly multidimensional).
Furthermore, such indexed properties can be overloaded:

.. code:: c++

   ref class RC {
     property double ip[double, int] {
       double get(double, int);
       set(double, int, double);
     }
     property float ip[float, int] {
       float get(float, int);
     }
   };

Because of this possibility, the symbol representing a property in a class has
a kind ``sk_property_set``, which reflects that it potentially represents a set
of properties (unlike function symbols, there is no separate symbol kind for
single properties and overloaded sets).  ``sk_property_set`` symbols point to a
structure of type ``a_property_set_symbol_supplement``, which in turn points
both to the associated pseudo-data-members (``sk_field`` or
``sk_static_data_member``) and to the overload sets formed by their ``get`` and
``set`` accessors respectively.

Indexed properties can have the name ``default``: Applying a subscript to such
a property's parent object amounts to a use of such a property (this has some
similarity to the more traditional ``operator[]``).  If any such
default-indexed properties are present in a class, the corresponding
``sk_property_symbol`` is pointed to from the class' symbol supplement.

C++/CLI has a notion of "event" members.  These are similar to C++/CLI
properties, but they cannot be indexed, must have handle-to-delegate type, and
they allow only three operations: ``+=``, ``-=``, and "call".  An event
definition can declare up to three accessors (matching the aforementioned three
operations) called ``add``, ``remove``, and ``raise``, or it can be "trivial"
(in which case those accessors are generated by the front end).  Since events
cannot be indexed, they cannot be overloaded, and no separate symbol kind is
needed for them: They're simply associated with the ``sk_field`` or
``sk_static_data_member`` symbol for their pseudo-data-member.

Class Object Layout
===================

Laying out the storage for a class means computing the overall size of an
object of that type and assigning to each of its components an offset
representing its position in the object.  Nonstatic data members are
represented by entries of type ``a_field``, each of which has an ``offset``
value that represents the byte offset from the beginning of the object and a
``offset_bit_remainder`` value (nonzero for bit fields only) that represents
the additional bits that must be taken into consideration if the field is not
aligned on a byte boundary.  Base classes have an ``offset`` field that
contains the byte offset from the beginning of storage for the class as a whole
to the beginning of storage for the base class's data section; for virtual base
classes there is also a ``pointer_offset`` field, which is the offset of the
pointer to the data section.  The class type supplement also has a
``virtual_function_info_offset`` field.  All these offset values together make
up the abstraction we refer to as the class's layout.

Object Layout Models
--------------------

The front end provides by default two distinct object models: a "classic" model
based on the original cfront implementation and a more modern "IA-64" model
based on specifications drafted by a group of vendors for the 64-bit Itanium
Architecture of Intel.  At the time of this writing, the latter specifications
can be found at:

   `<https://itanium-cxx-abi.github.io/cxx-abi/>`_

The implementation of both models can be found in ``layout.c``.  Where
convenient, the same routines are used to implement the two models (e.g., there
is only one ``do_class_layout`` function).

The front end can also be configured to implement small variations of both the
cfront-like and IA-64 models.  For the cfront-like model, increased
compatibility with the original cfront implementation can be achieved by
setting ``CFRONT_OBJECT_CODE_COMPATIBILITY`` to TRUE (this involves routines
whose names start with ``cfc_``).  Similarly, by setting
``DEFAULT_EMULATE_GNU_ABI_BUGS`` to TRUE, improved compatibility is achieved
with the default IA-64-based ABI implemented by GNU C++ 3.2 (and this involves
routines in ``layout.c`` whose names start with the ``gnu_`` prefix).

General Layout Steps
---------------------

All classes are laid out in the same general way, following this pattern (in
the sequence indicated):

* | First, in the IA-64 model, storage is reserved for the so-called *primary
    base class* (if it exists) or for a pointer to a virtual table (if the
    class is polymorphic without having a primary base):

  * | There is no primary base class concept in the cfront-like object model.
  * | The primary base class shares a "virtual pointer" at offset zero with the
      complete class.  Usually, the primary base class is a nonvirtual direct
      base class, but it can also be indirect and/or virtual in less common
      situations.  If none of the base classes are polymorphic, the derived
      class has no primary base class.  The primary base class is also pointed
      to from the class type supplement (by the field ``primary_base_class``).
  * | No storage is allocated for non-primary virtual bases of the primary base
      class at this point.
* | Next, storage is allocated for all the remaining *direct* base classes
    (which implicitly includes storage for nonvirtual *indirect* base
    classes as well):

  * | In the cfront-like model, when the direct base class is also virtual,
      only enough space is set aside for a pointer to the actual storage,
      which appears later.  In the IA-64 model, no such pointer is used and
      -- unless it is also the primary base -- the direct virtual base is
      allocated later on.
  * | In the case of a nonvirtual direct base class, enough storage is set
      aside for its own primary base class (if any), its nonvirtual base
      classes, its virtual base class pointers (in the cfront-like model), its
      own fields, and its virtual function information, but no space is
      allocated for its virtual base classes.
  * | In the cfront-like model, nonvirtual direct base classes that are *empty*
      (no own or inherited nonstatic data members and no virtual functions or
      direct or indirect virtual base classes) are allocated in a separate pass
      when ``targ_optimize_empty_base_class_layout`` is TRUE.  In the IA-64
      model, direct empty bases are allocated in the same pass as nonempty
      bases.  (See :ref:`empty-base-class-optimization`).
* | Next, storage is reserved for the class's own fields.
* | Next, in the cfront-like model, storage for virtual function information
    (typically, a pointer to a virtual function table).
* | Finally, storage for its virtual base classes, with space enough in each
    case for its own primary and nonvirtual base classes, virtual base class
    pointers (cfront-like model only), fields, and virtual function
    information.

The storage and alignment for all the items but the last are recorded in the
class's type supplement as ``size_without_virtual_base_classes`` and
``alignment_without_virtual_base_classes``; the ``size`` and ``alignment``
fields of the type entry itself record the values for the entire object.
Except in an incomplete type, both size values will be at least 1.

This general sequence is implemented by ``do_class_layout``, which is called by
``scan_class_definition`` once all the members have been scanned.
``do_class_layout`` calls several subroutines:

* | ``set_offsets_for_nonvirtual_base_classes`` to assign the offsets for
    nonvirtual base classes;
* | ``set_offsets_for_fields`` to set the ``offset`` and
    ``offset_bit_remainder`` values of each field;
* | ``set_offset_for_virtual_function_info`` to record the position at which
    (typically) the virtual function table pointer will be found;
* | ``set_offsets_for_virtual_base_classes`` to lay out the part of the class
    object where the data sections of virtual bases classes are kept; and
* | ``set_offsets_for_indirect_base_classes`` to record the offsets of indirect
    nonvirtual base classes (this phase is really just a fixup pass -- no new
    storage is reserved).

.. _empty-base-class-optimization:

The Empty Base Class Layout Optimization
----------------------------------------

The C++ language specifications allow an implementation to allocate an empty
base subobject at the same location as another subobject provided this does not
cause two objects or subobjects of the same type to be allocated at the same
address.  For example:

.. code:: c++

   struct E {};
   struct A: E { char c; };
   struct B: E { E e; };

In ``A``, the base class ``E`` can be allocated at the same offset as the field
``c``, but in ``B`` the optimization cannot be applied because two subobjects
of the same type (the field and the base) would occupy the same address.

In the cfront-like model, the front end implements this optimization when
``targ_optimize_empty_base_class_layout`` is TRUE and most of the required work
is performed in ``set_offsets_for_empty_nonvirtual_base_classes``.  The
cfront-like optimization heuristic uses a two-pass algorithm:

* | First the direct nonvirtual nonempty direct base classes are allocated as
    usual; i.e.  empty base classes are skipped.
* | Next, an additional pass is made in
    ``set_offsets_for_empty_nonvirtual_base_classes`` to place the direct
    nonvirtual empty base classes.  The first offset to be attempted is the
    origin of the object (i.e., zero).  If a situation is detected that
    would cause two subobjects with the same type to be allocated at the
    same offset, a new offset is sought after the next allocated base (if
    one is available).  This is repeated for every empty base -- without
    backtracking the offset -- until there are no more empty bases.
  | Another example illustrates how the second pass is a single pass (i.e.,
    the offset if not backtracked):

  .. code:: c++

     struct E1 {}; // Empty
     struct E2: E1 {}; // Empty
     struct E3 {};
     struct B { E1 e; } // Not empty
     struct D: E1, E2, E3, B { char c; };
     // Layout:
     //   B : this+0. (a nonempty base is allocated in the first pass)
     //   E1: after B because at this+0 it conflicts with B::e.
     //   E2: at least one byte farther than direct base E1 because else
     //          E2::E1 conflicts with base E1.
     //   E3: same offset as E2 since there is no type conflict; there is,
     //       however, no backing up to the offset of direct base E1.
     //   c : same offset as E2 and E3.

  | The optimization can only be applied to empty base classes and not to empty
    (nonstatic) data members.  Hence, ``B`` cannot be considered empty in this
    last example.

In the IA-64 model, empty base classes are allocated in the same pass as other
direct nonvirtual base classes.  In this case the heuristic is different: If
the base can be allocated at offset zero without causing a conflict with a
subobject that has already been allocated there, then that is where the empty
base class is allocated.  Otherwise, it is allocated at the next available
offset.  Every time a base class is allocated (whether empty or not),
``base_subobject_conflict`` is called: If a conflict is detected, the
allocation is retried at the next alignment boundary.  This process is repeated
until there is no conflict.  A similar process is needed for the first field
(to avoid having an empty base of a field end up at the same offset as an empty
base of the complete class).

The IA-64 model also handles the concept of a *nearly-empty base class*: This
is a virtual base class whose storage requirement is limited to a virtual table
pointer and (optionally) its own virtual base classes.  Such a class can be
selected as a primary base class, in which case it is allocated at offset zero.

Deferred Class Fixup
====================

As mentioned above, default arguments, inline member function bodies, and field
initializers are not processed until the entire class definition has been seen;
and when the class is a nested class, the processing is deferred until the
definition of the outermost containing class has been completed.

However, it gets even more complicated when templates are involved.  Consider
this example:

.. code:: c++

   template <class T> class A {
     void f(T* = new T) { ... }
   };
   class B {
     A<B> x;
   } b;

Here, the instantiation of ``A<B>`` is kicked off before the definition of
``B`` has finished, but the default argument expression for ``A<B>::f(B*)``
requires that ``B`` be complete.

To accommodate cases like this, the default argument and inline function body
processing is deferred until all pending class definitions have completed.
(The definition of ``B`` is pending when the definition of ``A<B>`` is
finished, so the fixup for the latter is deferred.)

``scan_class_definition`` calls
``process_deferred_class_fixups_and_instantiations``, and if global variable
``pending_class_definitions`` is zero,
``process_deferred_instantiation_requests`` (in ``templates.c``) and
``process_deferred_class_fixups`` are called.

The latter calls ``delayed_scan_fixup_for_class``, which scans the token caches
saved for default arguments and function bodies.  First it pushes a class
reactivation scope.  Then, for default arguments it pushes a function prototype
scope and calls ``delayed_scan_of_default_arg_expr`` (or, for class template
instances, ``delayed_scan_for_function_template_default_args``).  For function
bodies it calls ``scan_function_body`` -- except that the scanning of inline
member function bodies inside a class template instance is further deferred
until the member function has actually been referenced.

A similar mechanism is also used to delay processing of in-class initializers
for nonstatic data members in C++11 mode and of static data members of C++/CLI
managed classes; see ``record_inclass_initializer_fixup`` and
``inclass_initializer_fixup_for_class``.

Class Linkage
=============

Like functions and variables, a class has linkage recorded in the
``source_corresp.name_linkage`` field of its type entry.  Ordinarily (i.e.,
except in cfront-compatibility mode), when a class is first created, it is
given external linkage by default, except for local classes (which have no
linkage) and classes declared inside an unnamed namespace (which have internal
linkage).  This setting in turn determines the linkage and storage class
applied to class members.

In cfront mode, however, when a class is first created, it is given internal
linkage by default, and, since the class and its members are supposed to have
the same linkage, its members also have internal linkage (and correspondingly a
storage class of ``static``).  These are provisional settings, however, since
the presence of a non-inline member function or a static data member requires a
class to become externally linked.  Moreover, if a class is referenced in a
declaration of an externally linked entity, it needs to have external linkage.
Thus, at the end of translation unit processing ``check_class_linkage`` is
called (in cfront-compatibility mode only) to make a pass over all the classes
defined in the translation unit and, where appropriate, to fix up the linkage
and storage class of classes and their members.

.. _lambdas:

Lambdas
=======

A C++11 lambda expression is essentially a shortcut to define a "function
object" class and initialize a temporary object of that class.  For example:

.. code:: c++

   int f(int x) {
     auto f_impl = [x]()->int { return x*2; };
     return f_impl();
   }

is largely equivalent to:

.. code:: c++

   int f(int x) {
     class _Closure {
       int x;
     public:
       int operator()() const { return x*2; };
     };
     _Closure f_impl = (_Closure){ x };
     return f_impl();
   }

The implementation of lambdas in the front end exploits this equivalence: When
expression processing encounters the beginning of a lambda expression, it hands
over the task of parsing the construct to ``scan_lambda`` (see
:ref:`scanning-operations`).  This routine calls ``make_closure_class``, pushes
an associated class scope on the stack (after captures have been scanned -- see
below), and records a synthetic ``a_class_def_state`` structure to go along
with the class definition scope.  This allows the lambda to be parsed much as
if it were an in-class definition of a member function.

``scan_lambda`` also creates the ``a_lambda`` entry and records it in the scope
stack entry for the closure class.  That allows, e.g., expression and statement
processing to be aware of the lambda context.  The convenience function
``get_current_lambda`` is also useful for this purpose.  The type entry for the
closure class does not itself directly point to the associated lambda (due to
memory region constraints), but its class type supplement has a flag indicating
that it is a lambda closure class type (rather than a user-defined class type).

Explicit and Implicit Captures
------------------------------

The C++11 lambda syntax allows for the optional specification of an implicit
capture mode and explicitly captured local variables and expressions (the
latter are often referred to as "init-captures").  ``scan_lambda_capture_list``
parses this construct and records it in the ``a_lambda`` entry.  Explicitly
captured variables are recorded with a call to ``add_lambda_capture`` and
explicitly captured expressions are handled by a call to ``scan_init_capture``.
The fields associated with these explicit captures are added by a call to
``decl_lambda_capture_fields`` (after the closure class definition scope has
been pushed and after the optional lambda declarator has been processed).

If an implicit capture mode has been specified (using the "``[=`` ...  ``]``"
or "``[&`` ...  ``]``" forms of the lambda capture list),
``add_lambda_capture`` is also the routine that records the capture (it also
creates the associated field in the implicit capture case).  However, in such
cases we get there via ``lambda_capture_for_variable``, which is called from
the expression scanning routines while parsing the lambda body (ultimately, via
``scan_identifier``\ --see :ref:`scanning-idents`).

The "Lambda Declarator"
-----------------------

The syntax to describe lambda parameters, return type, and mutability is
optional and, when present, it differs from the ordinary member function
declaration syntax.  Still, the function ``scan_lambda_declarator`` is much
simplified by reusing ``function_declarator``.  (``function_declarator`` is
also used when parsing ordinary functions/operators, and is aware of the
enclosing class definition environment.) Similarly,
``decl_call_operator_for_lambda`` -- which creates the ``operator()`` member of
the closure class -- mostly relies on ``decl_member_function`` and
``decl_member_function_template``.

For generic lambdas (a C++14 feature), the lambda declarator corresponds to the
declarator of a function template, where the template parameters are created
implicitly from the use of ``auto`` as a parameter type.  For example:

.. code:: c++

     [](auto p) { return p*2; }

creates a closure class with an ``operator()`` that is a member template
corresponding to:

.. code:: c++

     template<typename _T> auto operator()(_T p) { return p*2; }

The front end implements this by prescanning the lambda declarator to record
information about ``auto`` parameters.  If there are any such parameters,
``set_up_generic_lambda_declarator_scan`` then proceeds to set up a member
function template declaration environment.  ``decl_specifiers`` also uses the
information gathered during the ``prescan`` to map occurrences of ``auto`` in
the parameter list to the appropriate template parameter type (see
``process_generic_lambda_param_type``).

``a_func_info_block`` has been augmented with a pointer to the associated
``a_lambda`` when applicable, and some lambda-specific behaviors of
``function_declarator`` (e.g., ``mutable`` as a function qualifier instead of
cv-qualifiers) make use of it.

If no return type is specified explicitly for the lambda, the return type is
temporarily set to the unknown type (``tk_unknown``).
``scan_return_expression`` is aware of this arrangement, and it will update the
return type if appropriate.  After the lambda body has been scanned
``scan_lambda`` sets the return type to ``void`` if no return statement was
seen (through ``check_implicit_lambda_return_type``, which also performs other
language-mandated checks if the return type was implicitly determined by a
return statement).

Scanning the Lambda Body
------------------------

The lambda body is parsed by ``scan_function_body`` as would the body of a
member function of a user-defined class type (or via
``function_prototype_instantiation``, in the case of a generic lambda).  There
are however a few subtle differences involved for the lambda case.

First, ``scan_function_body`` is called while the class scope is still active.
This is needed so that closure fields associated with implicit captures can be
generated.  So the option ``SFB_NO_CLASS_REACTIVATION`` is passed to
``scan_function_body`` to avoid reactivating the scope of the not-yet-completed
closure class.

Second, if IL lowering is enabled, the function body is not lowered when the
associated function scope is popped.  Instead, that lowering is triggered by a
call from ``scan_lambda`` to ``finish_function_processing_for_memory_region``.
This is a consequence of the previous observation: Lowering of the member
function cannot happen when the associated member function is popped because
the parent (closure) class hasn't been completed yet.

.. [#f1] For the most part the term "class" will be used generically in this
         chapter to include structs and unions as well as types declared with
         the ``class`` keyword.
.. [#f2] See :ref:`type-scope-and-symbol`.
.. [#f3] This rule does not apply in ``pcc`` mode, only in ANSI C and C++.
.. [#f4] In C mode a ``struct`` does not cause a new name scope to be
         established, and so ``decl_scope_level`` for a nested ``struct``
         will be the scope in which the parent ``struct`` is contained.  On
         the other hand, in C++ where a new name scope *is* established for
         classes, ``decl_scope_level`` is the scope of the class itself.
.. [#f5] This field is used in this manner during front end processing only.
.. [#f6] For instance, to create a virtual function table.  (This would be the
         typical use of this information, but the front end tries to avoid
         introducing any constraints that would imply a particular
         implementation of virtual functions.)
.. [#f7] To take this example a step further, if there were a declaration of
         ``C::f()``, the overriding virtual function entry for ``A::f()`` (the
         one that is associated with the base class for ``A`` in ``C``) would
         have to be modified, since it now would be overridden by ``C::f()``.
         This is discussed later.
.. [#f8] "Sealed" is called "final" in other dialects; the IL uses flags named
         ``final`` to indicate this property.
.. [#f9] Computing the field's storage properties (alignment and offset within
         an object of the class to which it belongs) is done as the class is
         being laid out; see ``set_field_size_and_offset`` in ``layout.c``.
.. [#f10] As a consequence, the C++11 ``auto`` type specifier cannot be used
          for these members since the type of the member could not be deduced
          from the initializer.
.. [#f11] This functionality is based on an extension offered by the GNU C++
          compiler and was added before a similar feature was proposed for the
          official language -- namely, ``const`` static data members with
          in-class initializers.
.. [#f12] For nonlocal classes, of course, the "innermost enclosing nonclass
          scope" and the "innermost enclosing namespace scope" will be one and
          the same.
.. [#f13] See ``have_proj_access_to_symbol`` in ``symbol_tbl.c``.
