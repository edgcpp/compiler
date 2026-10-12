..
  The following substitutions are used to align code that must be written
  without using a code block because of embedded formatting. |_| is a
  Unicode em space, while |.| is an en space. A combination of the two in
  the leading white space of a line can closely approximate the desired
  alignment of the following text.

.. |_| unicode:: U+2003
.. |.| unicode:: U+2002

.. _il-lowering:

===========
IL Lowering
===========




When we started developing the C++ front end, one of the first design decisions
we had to make was what intermediate language it would generate.

We already had a C front end (supporting the C89 version of the language) and a
Fortran front end.  The C front end generated an intermediate language that
looked a lot like C, and the Fortran front end generated an intermediate
language that looked a lot like Fortran.  The two intermediate languages were
compatible in the sense that they had the same structure and could be combined
into an intermediate language that is the union of the two.

It seemed clear that the C++ intermediate language ought to be compatible with
the other two languages in that same sense, and that in fact it should be a
superset of the C intermediate language, since C++ is (almost) a superset of
C89.  (When we talk about the C intermediate language or "C IL", we generally
refer to the IL to represent C89 constructs, not to the extensions supporting
the current C99 standard and other dialects like GNU C.)

But how much of a superset? Since our IL philosophy for Fortran and C had been
to make "high-level" ILs that were very close to the source language, our
initial inclination was to make an intermediate language with lots of new C++
features that would be the same sort of thing for C++.  It seemed like the
obvious approach.  When we analyzed that idea, and discussed it with customers,
however, we realized a few things:

* | There's a big semantic jump from C to C++.  Some of the C++ features are
    much more complicated than C or Fortran features.  Implementing them in a
    back end would add a *lot* of work, and requires a lot of knowledge of the
    C++ language.
  | Since our corporate mission is to deliver front ends that take care of
    all of the language issues and the attendant grief so that back ends
    can just worry about code generation (and *its* attendant grief), it
    didn't seem that we would be doing our full job by generating the
    high-level C++ IL.
* | We had customers that had existing C back ends and were interested in
    adding C++ with minimum development cost.

Okay then, what about generating an IL that's very close to the C IL? After
all, because of USL's cfront, people view C++ as a language that can be
translated into C, maybe even as a macro language on top of C.  Well, there are
problems with that, too:

* | It hides information that is useful in generating symbolic debugging
    information.  (This has historically been a weakness of cfront-based
    compilers, forcing programmers to debug using the generated C code.)
* | It makes it harder for a back end to use unconventional or aggressive code
    generation techniques for C++ constructs.
* | It makes the front end less suitable for use in source-analysis tools (they
    want to analyze the C++ source, not some version of it translated into C).

We considered various levels of intermediate language between the full C++ and
full C extremes, and all were unsatisfactory in some way.  Then David Callahan
of Tera Computer suggested that we do it both ways, that we generate a full C++
IL and also supply a piece of software that translates that IL into the
existing C IL.

Thus was born IL lowering.  The front end proper generates a true C++ IL, and
IL lowering translates that IL into C IL, which can then be fed into a C back
end or the C-generating back end we use for testing.  It should be noted that
although "lowering" is what's done, the output is still "high-level" -- it's
not as high-level as it was, but it's still at the level of the intermediate
language for C89.

The IL lowering pass is optional.  It need not be used if it's not wanted.  The
IL utilities work with both versions of the IL.

Since many C++ features go into IL lowering as abstractions and come out as C
code, IL lowering is actually choosing implementations of those features.  The
implementations are at the same level as those in cfront.  That is, they are
relatively "standard" implementations.  As one would expect, the generated C
has some of the same complex unreadable quality that cfront output has, and the
same ridiculously complex expressions.  The front end can even be configured to
produce constructs that are object-code compatible with cfront's.
Alternatively, the front end can generate code compatible with the IA-64
(Itanium) ABI.

IL lowering can also do inlining, though we firmly believe that that is
something that only a back end can really do properly.  [#f1]_

The IL lowering pass seems to solve the problems raised:

* | Both the C++ and C forms are available for those who prefer one form or the
    other.  In fact, one can use both forms, e.g., one could generate symbolic
    debugging information from the C++ form and object code from the C form.
* | It allows customers to get a C++ compiler going in minimal time.
* | If the features implemented by IL lowering require some minor changes, the
    changes can be made in IL lowering, which is relatively small, rather than
    in the front end as a whole, which is quite large.
* | If a customer wants to attach a back end to the unlowered IL (either
    initially or for an improved release of a compiler), the IL lowering code
    serves as a guide to the tasks that must be done.

Even though IL lowering will rewrite uses of non-C89 features, the C++-specific
fields of the IL are still present in the IL entries.  Back ends should not
count on those fields being cleared; rather, they should ignore them and
consider only the fields that C uses.  Viewed that way, the lowered IL should
contain nothing that would not show up in a C intermediate language tree.

One catch: Dynamic initialization of entities in the middle of blocks is left
in its original form.  This is not "incorrect" C IL, but is something that does
not come up in C89 programs.  It was judged that back ends would be capable of
dealing with the ``stmk_init`` statements even if they appear after other
executable statements in a block, and the alternative was to make lowering of
initialization of aggregates to constants quite messy.

The code for IL lowering is in ``lower_il.c``, and the associated declarations
are in ``lower_il.h``.  Name mangling code is in ``lower_name.c`` and
``lower_name.h``.  Code for lowering initializations, new/delete, and
constructors/destructors is in ``lower_init.c`` and ``lower_init.h``.  Code for
lowering exception handling features is in ``lower_eh.c`` and ``lower_eh.h``.

Certain higher-level constructs -- like variable-length arrays and complex
floating-point types -- from more recent C dialects (primarily C99, but also
GNU C) can also be lowered to C89-level IL.  The code for this is done
primarily in ``lower_c99.c``.  See :ref:`lowering-c-dialects` for an overview
of the constructs in this category that can be lowered.

Finally, IL lowering can also ensure that boolean controlling expressions
(e.g., the expression controlling which way an if-statement goes) always
produce an integer value 0 or 1 (as opposed to "zero or nonzero" or "null or
nonnull").  This normalization -- enabled by setting
``LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS`` to TRUE -- can occur in
all modes (C++, C89, etc.) and usually involves adding a "``!= 0``" comparison
to the controlling expression.

ABI support
===========

The front end and IL lowering support two ABIs (Application Binary Interfaces).
An ABI is a set of conventions for the runtime environment, which prescribes
class layout, access to virtual functions, name mangling, etc.  Code generated
by two compilers that are compatible with a given ABI should, in theory, be
able to interact properly, and with libraries that are compatible with that
ABI.

Up through version 3.0, the front end supported a single ABI and some minor
variants thereof.  This ABI, referred to as the "cfront-like ABI," is an
extended version of the original ABI used by cfront.  The extensions were
invented by EDG, and therefore the ABI does not conform to any industry
standard, other than in its general compatibility with what was once a *de
facto* standard.  The variants of the cfront-like ABI are:

* | cfront 3.0.1 compatibility, selected by setting
    ``CFRONT_3_0_OBJECT_CODE_COMPATIBILITY`` to TRUE.
* | cfront 2.1 compatibility, selected by setting
    ``CFRONT_2_1_OBJECT_CODE_COMPATIBILITY`` to TRUE.
* | If neither of those variants is selected, the default ABI is cfront-like
    with some minor quirks eliminated, principally with respect to allocation
    of space for virtual base classes.  This variant was originally intended as
    the default and best ABI, but in practice it hasn't been used very much,
    and the IA-64 ABI (see below) is a better choice.

As of version 3.1, the front end also supports the IA-64 ABI, described by
https://itanium-cxx-abi.github.io/cxx-abi/abi.html.  This is a modern ABI
originally designed for the Intel IA-64 ("Itanium") architecture, but now an
industry standard widely used on other architectures as well.  In particular,
the GNU g++ compiler uses the IA-64 ABI on many platforms.  This ABI is
selected by setting ``IA64_ABI`` to TRUE.  Note that the fully-lowered
implementation of exception handling provided by EDG works with the IA-64 ABI,
but does not conform to the spec, and that versions using the C-generating back
end do not fully conform because of some layout cases not easily represented in
C.

Note that the IA-64 ABI requires linker support in the form of a feature (often
called COMDAT) that allows different object files to contain (identical)
definitions of certain functions and variables.  The linker discards the
duplicates, ending up with a single definition.

The ABI support in the front end changes from time to time as features are
added and bugs are fixed.  The macro ``ABI_COMPATIBILITY_VERSION`` can be set
to a version number of the front end to keep the ABI frozen as it was at that
version.  Source language features that require ABI changes made after that
version are disabled.

What Does C++ IL Lowering Do?
=============================

IL lowering is called at the end of processing of each "top-level" scope (i.e.,
once at the end of each top-level function, and once at the end of the file
scope).  It rewrites any C++ IL constructs in the scope as semantically
equivalent C IL constructs.  (A similar process occurs for C99 IL; see
:ref:`lowering-c-dialects` for details.)

IL Lowering is suppressed if there are errors in the source program, if the
dialect of C language being compiled doesn't require it, if preprocessing only
is being done, or if syntax-checking only is being done.

Name Mangling
-------------

(Code for name mangling is in ``lower_name.c`` and ``lower_name.h``.)

"Name mangling" means transforming the names of C++ entities so that the names
include information on aspects of the entity's type and fully qualified name.
This is made necessary by the fact that the object code and C environments into
which a C++ program is translated typically contain fewer and simpler name
spaces than there are in the C++ language.  Specifically:

* | Overloaded function names are not allowed in C, and probably not in object
    code.
* | Classes have their own scopes in C++, but not in the generated C form.  An
    entity ``x`` from inside a class must not conflict with an entity ``x``
    from the file scope, for example.
* | External names in C and (probably) in the object code form a completely
    flat name space.  The names of C++ entities with external linkage must be
    projected onto that name space so that they do not conflict with one
    another.  A function ``f`` from a class ``A``, for example, must not have
    the same external name as a function ``f`` from class ``B``.
* | Some C++ "names" are not names in the conventional sense of the word
    (they're not strings of alphanumeric characters), e.g., ``operator=``.

We can see that there are three problems here:

* | Generating C code names that will not clash for non-external entities.
    This is only a problem when generating C code.
* | Generating external names that will not clash.  This is a problem whether
    the output is C or object code.
* | Generating alphanumeric names for entities with strange "names" in C++.
    This is a problem whether the output is C or object code, at least for
    external names.

As it happens, the name mangling process solves all three problems.  It also
solves the problem of generating hidden names for some behind-the-scenes
language support in such a way that they will match up across separate
compilations (e.g., virtual function table names).

Once name mangling has been done, the name linkage of C++ external entities is
changed to C external.

Cfront-like ABI name mangling
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The cfront-like ABI name mangling algorithm is the same as the one used by
cfront, and also matches the description in 7.2.1c of the ARM (except for some
minor details).  The reader is referred there for a complete description.
Here, we just hit the highlights and the things that differ from or are
extensions to that description.

Each type has a corresponding mangled encoding.  For example, a class type
is represented as the class name preceded by the number of characters in
the class name, as in ``5abcde`` for ``abcde``.  Simple types are encoded
as lower-case letters, as in ``i`` for ``int`` or ``f`` for ``float``.
``long long``, ``wchar_t``, and ``bool``, which do not appear in the ARM,
are encoded as "``L``", "``w``", and "``b``", respectively.  The C++11
types ``char16_t`` and ``char32_t`` are encoded as "``g``" and "``k``",
respectively.  A ``std::nullptr_t`` type is encoded as "``n``", while the
C++/CLI ``__nullptr`` is encoded as "``j``".  (In some Microsoft modes,
there are distinct integral types named ``__int8``, ``__int16``,
``__int32`` and ``__int64``.  They are encoded as "``m1``", "``m2``",
"``m4``", and "``m8``", respectively.) 128-bit integers (in GNU
compatibility mode) are mangled with "``m16``" and "``Um16``" (signed and
unsigned, respectively).  Type modifiers and declarators are encoded as
upper-case letters preceding the types they modify, as in ``U`` for
unsigned or ``P`` for pointer.  In GNU C++ mode, complex types are encoded
with an "``x``" preceding the code for the corresponding real
floating-point type (e.g., "``xf``" for "_Complex float").  The ``long
double`` type is indicated by an "``r``" encoding.  In addition to the type
declarators mentioned in the ARM (i.e., "``P``" for pointer, "``R``" for
reference, "``A``" for array, "``F``" for function, and "``M``" for
member), an "``E``" encoding has been added to indicate an rvalue
reference.  For ref-qualified member function types, an optional "``_R``"
(for an lvalue req-qualifier) or "``_E``" (for an rvalue ref-qualifier) can
follow the "``F``" that indicates a function type.  When C++/CLI is
enabled, the "``Ht``" encoding is used for tracking references, "``Hh``" is
used for handles, "``Hi``" is used for ``interior_ptr``, and "``Hp``" is
used for ``pin_ptr``.  ``__underlying_type`` (enabled when type trait
helpers are enabled) is mangled as "``o``".  See
``mangled_encoding_for_type``.

Types specified using the C++11 keyword ``decltype`` are encoded in one of
three ways.  When a ``decltype`` expression is either type-dependent or
value-dependent (i.e., it has some component that could be affected by the
C++11 SFINAE rules), it is said to be instantiation-dependent and it is
encoded using the character "``y``" or "``Y``" followed by a mangled
encoding of the expression ("``y``" is used when the expression is an
unparenthesized id-expression or a class member access; "``Y``" is used
otherwise).  ``decltype`` expressions that are not instantiation-dependent
are encoded using the known underlying type or value (e.g.,
``decltype(sizeof(int))`` is mangled using the mangled encoding for the
``size_t`` type, i.e., "``Ui``" in a front end that is configured to use
``unsigned int`` to represent ``size_t``).  In a manner similar to
``decltype``, ``typeof``\ (*type*) is mangled using the encoding "``t``"
and ``typeof``\ (*expression*) is mangled using the encoding "``p``".  The
"``u``" mangled encoding is also introduced in C++11 to represent the use
of ``auto`` as a type specifier when it appears in a *new-expression* that
is mangled because it appears in the context of an instantiation-dependent
expression (e.g., ``decltype(new auto(p1))``).  The letter "``n``" is used
to encode the type for ``nullptr`` (i.e., ``std::nullptr_t``).  The "``q``"
mangled encoding is used for the ``decltype(auto)`` notation.

When a function parameter is a C++11 function parameter pack, its type is
represented by the encoding "``Dp``" followed by the function parameter type.

Nested class types are encoded as a "``Q``" followed by a digit indicating the
depth of nesting, followed by a "``_``", followed by the mangled-form names of
the class types in the fully-qualified name of the class, from outermost to
innermost:

.. code:: c++

   class A {
     class B {  // Q2_1A1B
     };
   };

The name of the nested class itself is mangled to the form described above with
a prefix ``__``, which serves to make the class name distinct from all user
names.  The same mangling technique is used for namespaces.  In cases where the
mangled entity is specified in the source code as being in the global scope
(e.g., ``p->::X::y``), and the entity is a selection or call operand, the
global scope indication is specified using the letter "``G``" as the first
qualifier in a qualified name; for example:

.. code:: c++

     int g(int);
     template <class T> auto f(T p1) -> decltype(::g(p1));
                                        // f__tm__2_i__FZ1Z_YOcl_2_4g__GI1IO

Local class names and scoped enumerators are encoded by following the name by
"``__L``" followed by a number (which has no special meaning; it uniquely
identifies the type within the function scope).  This example (which requires
--c++11) shows a local class and its mangling within a function template:

.. code:: c++

   template <typename T> void f(T) {}
   void x() {
     struct A {} a;  // __12A__L1__x__Fv
     f(a);           // f__tm__15_12A__L1__x__Fv__FZ1Z_v
   }

This form is used when encoding the local class name as a type.  It's not
actually necessary to mangle the name of the local class itself unless it's
also a nested class.  See ``mangled_type_name_full``.  This is not in the ARM,
and cfront does it slightly differently.

The "``__L``" indicator is also used in the names of entities promoted out of
functions.  There, the "``__L``" appears between the name of the entity and the
mangled name of the function:

.. code:: c++

   void *f() {
     struct A {};           // __12A__L1__f__Fv
     static struct A a;     // a__L0__f__Fv
     return &a;
   }

The instance number (after the "``__L``") serves to ensure that the mangled
name is unique (and is chosen so as to make it repeatable across translation
units in cases where the ODR requires it).  Note that in configurations where
local types cannot be used in template arguments (i.e., other than --c++11 or
when microsoft_mode >= 1400) and therefore can't appear in external names, the
mangled names may differ slightly from those listed above.

Unnamed namespace names are mangled as ``__N`` followed by a unique string.
The unique string is the "module id" generated by IL lowering from the source
file name and some other information, but from the perspective of name mangling
it is simply ignored.  Such names appear only in contexts where their length is
given, so it's easy to know how much to ignore.

Template classes have mangled names that encode the arguments of the template:

.. code:: text

   template<class T1, class T2> class abc {};
   abc<int, int> x;
   abc__tm__3_ii
              ^^--- Two template arguments of type int.
            ^------ Total length of template argument list string, including
                      the underscore.
        ^^--------- Fixed string, indicates "template".
   ^^^------------- The name of the class template.

When distinct mangling for templates is disabled, "``tm``" is replaced by
"``pt``" (which is what cfront uses).

The encoding for nontype template arguments encodes both the parameter type and
the actual argument value:

.. code:: text

   template<int I> class def {};
   def<5> x;
   def__tm__7_XCiL15
                   ^-- Literal constant representation.
                  ^--- Length of literal constant.
                 ^---- "L" indicates literal constant, "c" indicates address
                          of variable, etc.
               ^^----- Type of template argument, with "const" added.
              ^------- "X" indicates beginning of constant argument.

Template argument packs are introduced by "``__pk__``":

  .. code:: text

     template <class T, class ...U> int f();
     int x = f<short, int, double>();
     f__tm__12_s__pk__3_id__Fv_i <-- encoding for "f<short, int, double>()"
                               ^---- Return type (int).
                             ^------ Argument list (void).
                            ^------- Function.
                         ^---------- Second argument of the pack (double).
                        ^----------- First argument of the pack (int).
                      ^------------- Total length of template argument pack.
                  ^^---------------- Beginning of a template argument pack.
               ^-------------------- First template argument type (short).
            ^^---------------------- Total length of template argument list.
        ^^-------------------------- Introduces template arguments.
     ^------------------------------ The name of the function template.

Constants are encoded with the letter "``C``" followed by the type of the
constant and the encoding of the constant value (e.g., "``CiL_2_59``"
represents the integer constant 59).  There are several different encodings
for different kinds of constants:

* | Integer:

  .. code:: text

     L3n12  <-- encoding for "-12"
        ^^----- Literal value.
       ^------- "n" indicates negative.
      ^-------- Length of the literal.
     ^--------- "L" indicates a number.

  | This is compatible with cfront 3.0.1.
  |
  | The length of the literal uses an encoding compatible with cfront, i.e.,
    it's either a single digit or two digits followed by an underscore (e.g.,
    "``L10_1234567890``").  This encoding has some ambiguity problems when the
    template argument is followed by an underscore.  Such cases did not come up
    in the cfront world, but they do come up when template arguments are
    applied to template functions (see paragraphs on distinct mangling for
    templates, below).  To deal with this, a new unambiguous mangling for
    literal lengths has been introduced as of version 2.32.  It has the length
    enclosed in underscores in all cases (single- or multi-digit), e.g.,
    "``L_1_2``".  This form is used in the new contexts that didn't come up for
    cfront, and the old form continues to be used in the cfront contexts.
* | Float:

  .. code:: text

     L4n1p5 <-- encoding for "-1.5"
        ^^^---- Literal value ("p" for decimal point).
       ^------- "n" indicates negative.
      ^-------- Length of the literal.
     ^--------- "L" indicates a number.

  | cfront 3.0.1 does not implement this, so we made it up.  In actuality, the
    literal form of the floating constant depends on the ``fp_to_string``
    routine, so the encoding is likely to be longer (for example, it's likely
    to include an exponent).
* | Complex floating point literals are mangled like floating point literals
    above except that both real and imaginary components are included:

  .. code:: text

       L_3_0d0_3_1d1 <-- encoding for "0.0+1.0i"
                 ^^^---- Imaginary portion of complex number.
              ^^^------- Length of the imaginary portion of the number.
           ^^^---------- Real portion of complex number.
        ^^^------------- Length of the real portion of the complex number.
       ^---------------- "L" indicates a number.

* | Address of variable or routine:

  .. code:: text

     4abcd <-- encoding for address of "abcd"
      ^^^^---- Name of entity.
     ^-------- Length of the name.

  | This is compatible with cfront 3.0.1.
* | For pointers to data members, the offset value encoded as an integer:

  .. code:: text

     L212  <--- encoding for an offset of "12"
       ^^------ Literal value.
      ^-------- Length of the literal.
     ^--------- "L" indicates a number.

  | This is compatible with cfront 3.0.1.
* | For pointers to member functions, the ``__mptr`` triplet of values (delta,
    index, function or offset), encoded as follows:

  .. code:: text

     LM0_L2n1_1j
              ^^- Function name, or alternatively "0" if the pointer
                    to member uses an offset (e.g., LM0_L11_0).
         ^^^^---- Index value, encoded as an integer.
       ^--------- Delta value.
     ^^---------- "LM" indicates a pointer to member function.

  | This is compatible with cfront 3.0.1.  Note that "``0``" is always used
    for the offset, not the actual offset value.  This follows cfront.  The
    idea seems to be that "0" is really a way of saying "there is no
    function;" the offset value itself would not be of interest to a name
    demangler.
* | For strings, the type of the string is encoded.  The character "``S``"
    is used to indicate a string literal.  The string ``L"xyz"`` is encoded
    as "``CA4_CwLS``" (i.e., a string literal of type ``const wchar_t[4]``).
* | ``nullptr`` is encoded using the encoding for integers given above; the
    type is always "``n``" (for ``std::nullptr_t``), and the value is
    always zero, so the entire mangled encoding for ``nullptr`` is always
    "``CnL_1_0``".

The fact that there are rules for mangling type names seems to suggest that
names of types are mangled.  Strictly speaking, except for nested class names,
they are not.  It is really only the names of variables and routines that need
to be mangled.  The mangled form of type names does have a use, however: it is
used when building the mangled names of variables and routines, to represent
the types referenced by the entity (e.g., parameter types).

Unnamed (class/struct/union, enums, lambda closure) types are given a generated
name during mangling; that name is then used as described above (e.g., in
mangled names of variables and routines).  The names generated for these
unnamed types are constructed so as to ensure that types subject to ODR
constraints across translation units are given the same name; other types are
given names guaranteed not to collide with similar constructs in other
translation units.

Unnamed class/struct/union and enums are given the name "``__Ut``" followed by
an instance number (within the namespace/class/function scope) which makes the
generated name unique within that scope.  A mangled encoding of the type will
include its parent's encoding as well.

Lambda closure types are a special kind of unnamed class type, and they are
mangled with a similar "``__Ul``" encoding that additionally gives information
about the lambda's parameter types:

  .. code:: text

     __Ul1_Fif <-- [](int, float)->float {return 0.0;}(i, f);
           ^^^---- Lambda operator() function type (int, float).
         ^-------- Instance (within parent scope).
     ^^^^--------- "__Ul" indicates lambda.

Lambdas defined in certain contexts, specifically some default arguments and
member initializers, are subject to ODR constraints and therefore must get
names that can be generated identically in different compilations.  That's
accomplished by making the default argument or member initializer a
"pseudo-parent scope" and encoding the closure type as a member of that
pseudo-scope.

Lambdas defined in default arguments (and subject to ODR constraints) are given
a name based on this encoding:

  .. code:: text

       __Ud1_2_Fv__L0__f__FiT1 <-- void f(int i=[]{return 0;}(),int k=0)
                 ^^^^^^^^^^^^^---- Local to function f(int,int).
               ^^----------------- Lambda operator() function type.
             ^-------------------- Declared in trailing parameter 2.
           ^---------------------- Instance 1 (within parameter).
       ^^^^----------------------- "__Ud" indicates lambda in default arg.

Lambdas in member initializers are given a name that is prefixed with
"``__Um``":

  .. code:: text

     __Q3_10S__tm__2_i1x8__Um1_Fv < template <class T> int S<T>::x =
                                                            []{return 1;}();
                               ^^-- Lambda operator() function type.
                             ^----- Instance (within member initializer).
                         ^^^^------ "__Um" indicates lambda in initializer.
                        ^---------- Length of type name that follows.
     ^^^^^^^^^^^^^^^^^^^----------- Nested encoding for S<int>::x::.

The following kinds of entity names (only) are mangled:

* | Function names.  Even non-member function names are mangled, to deal with
    overloading.  Names of functions with ``extern "C"`` linkage are not
    mangled.  See ``mangle_function_name``.
  | Mangled function names have the function name followed by "``__``" followed
    by "``F``" followed by the mangled description of the types of the
    parameters of the function.  If the function is a member function, the
    mangled form of the class name precedes the "``F``".  If the member
    function is static, an "``S``" also precedes the "``F``".  In cases where
    the function has ``extern "C"`` linkage on the function type, a "``K``"
    modifier immediately follows the "``F``" indication.

  .. code:: c++

     int f(float);                   // f__Ff
     class A {
       int f(float);                 // f__1AFf
       static int g(float);          // g__1ASFf
     };

  | Members of namespaces are mangled like member functions.
  | For the Microsoft extension that allows explicit specification of a virtual
    function to be overridden, name mangling adds the class of the overridden
    virtual function as ``O`` followed by the class type encoding, after the
    class name.  So, for example, a member ``D::f`` that explicitly overrides
    ``B::f`` would be encoded as ``f__1DO1BFv``.
* | Special and operator function names, like constructors and ``operator=()``.
    The encoding is similar to that for normal functions, but a coded name is
    used instead of the routine name:

  .. code:: c++

     class A {
       int operator+(float);         // __pl__1AFf
       A(float);                     // __ct__1AFf
     };
     int operator+(A, float);        // __pl__F1Af

  | The ARM contains a full list of the name encodings.  Static constructors,
    used in C++/CLI, are encoded with "``__st__``"; finalizers are encoded with
    "``__df__``".  See ``mangled_operator_name``.
* | Static data member names.  The mangled form is the member name followed by
    "``__``" followed by the mangled form of the class name:

  .. code:: c++

     class A {
       static int i;                 // i__1A
     };

  | This type of mangling is also used for variables that are members of
    namespaces.  Generally speaking, the names of variables at the file scope
    don't need to be mangled, but that's not true for variable templates, or
    for variables that may have implicit or explicit ``abi_tag``\ s.  See
    ``mangle_variable_name``.
* | Functions that are instances or specializations of templates.  These
    are mangled to include the template signature.  This is controlled by
    the variable ``distinct_template_signatures``.  Template parameters
    (both type and nontype) are mangled as "``Z``\ *n*\ ``Z``" or "``Z``\
    *n*\ ``_``\ *m*\ ``Z``", where *n* is the template parameter position
    number (first is 1) and *m* is the template parameter depth (top level
    is 1, which is assumed in the case where it is not specified).  The
    return type of the function is also included in the mangled name.
    "``__S``" can appear either directly after the template name, to
    indicate that the template is specialized, or after the template
    argument list, to indicate that the instance is specialized.

  .. code:: c++

     template <class T> struct A {
       template <class T2> void f(T2);
     };
     template <> template <class T2> void A<char>::f(T2) {}
     template <> template<> void A<float>::f(int) {}
     A<int> ai;
     A<char> ac;
     A<float> af;
     int main(){
       ai.f(1); // f__tm__2_i__10A__tm__2_iFZ1_2Z_v
       ac.f(1); // f__S__tm__2_i__10A__tm__2_cFZ1_2Z_v
       af.f(1); // f__tm__2_i__S__10A__tm__2_fFZ1_2Z_v
     }

  |
  | The specialization indications are also placed in the names of parent
    classes of functions and static data members generated from templates.  A
    "``__S``" following directly after the class itself indicates that the
    class is specialized (and similarly for a class template), and a "``__S``"
    after the template argument list of a class template indicates that the
    instance is specialized.

  .. code:: c++

     template <class T> struct A {
       void f();
       struct B {
         void bf(T);
       };
     };
     template <> struct A<short> {
       void f();
     };
     template <> struct A<int>::B {
       void bf(int);
     };
     A<short> as;
     A<int>::B aib;
     main () {
       as.f();         // f__13A__tm__2_s__SFv
       aib.bf(1);      // bf__Q2_10A__tm__2_i4B__SFi
     }

  | When a partial specialization of a class template appears, the mangled name
    has two template argument lists.  The first (which begins with
    "``__ps__``") gives the template arguments that appear on the declaration
    of the partial specialization, and the second gives the arguments according
    to the template parameters of the partial specialization.

  .. code:: c++

     template <class T, class U> struct A {};
     template <class T> struct A<T *, int> {
       void f();  // f__23A__ps__6_PZ1Zi__tm__2_iFv_v
                  //      A  <T *, int>   <int>
     };

  | Where expressions appear in template signatures, they are mangled as in the
    following example:

  .. code:: text

     Opl_2_Z1ZZ2ZO <-- "Z1 + Z2", Z1/Z2 indicating nontype template
                       parameters.
                 ^---- "O" to end the operation encoding.
              ^^^----- Second operand.
           ^^^-------- First operand.
        ^^^----------- Count of operands (with bracketing underscores).
      ^^-------------- Operation, using same encoding as for operator
                       function names.
     ^---------------- "O" for operation.

  | The two or three letter operation encodings (e.g., "``pl``" in the example
    above) are the same as the operator function encodings specified in the ARM
    (without the leading double underscores).
  | The operands can be template parameters, constants (using the encoding
    described for nontype template arguments, not including the initial
    "``X``"), subexpressions (using the expression operation encoding described
    above), mangled names (e.g., of variables or routines), or (in ``decltype``
    expressions) function parameter references.
  |
  | In addition to those operations specified in the ARM, the following
    operation encodings have been added as EDG extensions:

  .. code:: text

     "ds" = .*
     "dt" = .
     "ps" = unary plus
     "ng" = unary minus
     "de" = * (indirection)
     "ao" = & (address of)
     "ppe" = prefix increment (pp indicates postfix increment)
     "mme" = prefix decrement (mm indicates postfix decrement)
     "tw" = throw
     "rl" = __real
     "im" = __imag
     "mn" = GNU min operator (<?)
     "mx" = GNU max operator (>?)
     "sp" = ... (pack expansion)
     "ht" = % (C++/CLI unary "%" operator which returns a handle)
     "sb" = [] (C++/CLI array subscript -- has variable number of operands)
     "gc" = gcnew (C++/CLI gcnew -- optional list of dimension expressions
            precedes a type operand)
     "cp" = () (call operation with ADL suppressed -- has variable number of
            operands)
     "il" = {} (brace-enclosed initializer list -- has variable number of
            initializers)
     "tl" = T{} (cast of a brace-enclosed initializer list -- has a type
            followed by a variable number of initializers)
     "nx" = noexcept
     "li" = literal operator (for user-defined literals -- has zero or one
            operand and is followed immediately by a count and the ud-suffix
            for the literal operator, e.g., operator "" _miles would be
            "li6_miles")

  | The following added operations differ from the standard operation encoding
    in that they include an initial type before a single operand.  The "``cs``"
    encoding is used to encode both the old-style cast (where there is one
    non-type operand) as well as the functional-notation cast (where there can
    be zero or more non-type operands).  The case where a functional-notation
    cast is used with one argument is indistinguishable from the old-style cast
    case:

  .. code:: text

     "sc" = static_cast
     "dc" = dynamic_cast
     "rc" = reinterpret_cast
     "cc" = const_cast
     "cs" = old-style (one argument) or functional-notation cast
     "sf" = C++/CLI safe_cast

  | The following operation differs in that it takes a single type argument:

  .. code:: text

     "ct" = C++/CLI ::typeid

  | The following operations differ in that their source can specify either a
    type or an expression:

  .. code:: text

     "sz" = sizeof
     "af" = __ALIGNOF__
     "uu" = __uuidof
     "ty" = typeid
     "sk" = sizeof...

  | The mangled encodings for these cases are:

  .. code:: text

        OszZ1Z0O <-- "sizeof(Z1)", Z1 indicating a template parameter.
               ^---- "O" to end the operation encoding.
              ^----- Count of operands, 0 for type and "e" cases, 1 for "X".
           ^^^------ Encoding for type or "e" or "X" (for expression cases).
         ^^--------- Operation ("sz" for sizeof, "af" for __ALIGNOF__,
                     "uu" for __uuidof, or "ty" for typeid)
        ^----------- "O" for operation.

  | There are three cases.  When the operation takes a type, then the type
    is encoded after the operation encoding and there are no operands.
    Prior to version 4.2, an expression was indicated with an "``e``" in
    place of the type (but no expression was contained in the mangled name
    and the operand count was still zero).  In version 4.2 and later, the
    mangled expression is included (operand count is 1), and the type is
    replaced with an "``X``" to indicate that an expression follows.  | | The
    ``new`` and ``delete`` operations (as well as ``new[]`` and
    ``delete[]``) are encoded thusly:

  .. code:: text

           v--------------vvvvvvvvvv----- These are optional.
        Onwg_1_CiL_2_10Z1Z_1_CiL_1_0O <-- encoding for "::new (10) T (0)"
                                    ^---- "O" to end the encoding.
                             ^^^^^^^----- Initializers (if any).  An optional
                                          "bi" indicates the use of a
                                          brace-enclosed initializer list.
                          ^^^------------ Optional initializer count (zero
                                          or more).  Omitted (along with
                                          initializers) if none were
                                          specified.
                       ^^^--------------- Type of new operation.
               ^^^^^^^^------------------ Placement arguments (if any).
            ^^^-------------------------- Placement argument count (zero
                                          or more).
           ^----------------------------- "g" indicates global new.
         ^^------------------------------ new operation (nw, nwa).
        ^-------------------------------- "O" for operation.

           v----------- This is optional.
        Odlg_1_I1IO <-- encoding for "::delete p1"
                  ^---- "O" to end the operation encoding.
               ^^^----- Argument to delete.
            ^^^-------- Argument count (always one for delete).
           ^----------- "g" indicates global delete.
         ^^------------ delete operation (dl, dla).
        ^-------------- "O" for operation.

  | Builtin operations (i.e., vendor-specific extensions like Microsoft
    ``__is_base_of``) use the operation "``bi``", and any operands that are
    types are prefixed by "``T``".  The encoding "``__dn``" was added to
    represent a destructor name (and is followed by a single type name).
    See ``mangled_encoding_for_expression`` for details on the mangling of
    expressions.  | | Where arrays with sizes based on template parameters
    appear, they are mangled as "``A_``\ *mangled-expression*\ ``_``\
    *element-type*" (the underscore following the "``A``" differentiates
    this case from the usual known-size array case).  | | A template
    template parameter that is followed by a template argument list, e.g.,
    ``ttparam<x,y,x>`` in source code, is represented by the ``Z...Z``
    encoding, with the template argument list encoding inserted before the
    closing ``Z``, as in ``Z1__tm__4_Z2ZZ``.  | | References to function
    parameters in ``decltype`` expressions (e.g., ``p`` in ``template
    <class T> auto f(T p) -> decltype(p)``) are mangled as such:

  .. code:: text

       v-vv----- These are optional.
      IC1_2I <-- "const param#1 two levels up"
           ^---- Terminating non-digit character so parameter number won't run
                 into an entity with an initial length.
         ^^----- Number of "levels up" for this parameter (0-based).  Omitted
                 if zero.
        ^------- Parameter number (1-based) or 0 for "this".
       ^-------- Optional cv-qualifiers.
      ^--------- "I" indicates parameter reference.

* | The implicit or explicit use of ``this`` in a mangled name is also
    represented with the function parameter mangling given above (where the
    parameter number is set to zero).
* | Names of variables generated for virtual function tables.  These have names
    like

       ``__vtbl__``\ *mangled-class-name*

  | or

       ``__vtbl__``\ *mangled-base-class-name*\ ``__``\ *mangled-class-name*

  | In the case of a virtual function table used during construction or
    destruction of a subobject, there can be an additional mangled class name
    on the end.  See ``make_var_for_virtual_function_table``.
* | Names of variables generated to contain runtime type information.  These
    have names like

       ``__T_``\ *type-encoding*

  | and

       ``__TID_``\ *type-encoding*

  | See ``mangled_typeinfo_name`` and ``mangled_id_object_name``.
* | Names of wrapper routines generated to deal with covariant virtual function
    return types.  These are pointed to from virtual function tables and have
    names of the form

       ``__VFE__``\ *overridden-class*\ ``__``\ *overriding-function*

  | See ``mangle_wrapper_name``.
* | Names of static variables and functions made external, e.g., because they
    are referenced from an instantiation and each instantiation is placed in a
    separate object file.  The form of the mangled names is

       | ``__STV__``\ *variable-name*\ ``__``\ *module-id*
       | ``__STF__``\ *mangled-function-name*\ ``__``\ *module-id*

  | where *module-id* is a unique name generated for the current compilation.
* | The GNU ``ifunc`` attribute, when lowered, creates a variable through which
    calls to the function are dispatched.  The variable uses the ``__IFV__``
    prefix:

       ``__IFV__``\ *variable-name*

* | When GNU function multiversioning is used, a set of routines with the same
    unmangled name are given unique mangled names based on their use.  The
    following prefixes are used to create these names:

      .. list-table::

       * - | ``__RES__``\ *mangled-function_name*
         - | resolver function
       * - | ``__IFC__``\ *mangled-function-name*
         - | ifunc function
       * - | ``__TGT__``\ *targets*\ ``__``\ *identifier*
         - | target-specific function

* | For classes and function names with the GNU ``abi_tag`` attribute, the
    "``__ab``" prefix is used (once for each string literal that is specified
    in the ``abi_tag`` attribute list):

       ``__ab``\ *<count><tag>*

  | where *<count>* represents the number of characters in the *<tag>* string
    literal.

* | Structured binding container variables (at namespace scope) are given
    mangled names based on the binding variable(s) they refer to.  The
    "``__SBC__``" prefix is used to identify this mangling.  Each of the
    binding variables, in order, are included in the container's mangled name,
    separated by "``__``".  A final "``__``" indicates the end of the
    structured binding container mangled name.  For example,
    ``__SBC__x__y______1N`` indicates a structured binding container for
    ``[x,y]`` in namespace ``N``.

Once a mangled name has been generated, a compression step examines it to see
whether it can be shortened by compression.  The compression approach is
simple-minded: the sequence "``J``\ *nnn*\ ``J``" means "repeat the sequence
beginning at offset *nnn* in the mangled name." The offset is 0-origined.  Only
sequences that begin with a length (e.g., ``7ABCDEFG``) are eligible to be
repeated in this way.  Each "``J``" in the original mangled name is replaced by
"``JJ``" to avoid ambiguities.  If compression is done, the mangled name is
given a prefix of "``__CPR``\ *nnnn*\ ``__``", where *nnnn* is the length of
the uncompressed form of the name.

IA-64 ABI name mangling
^^^^^^^^^^^^^^^^^^^^^^^

In the IA-64 ABI, name mangling is done according to the ABI spec
(https://itanium-cxx-abi.github.io/cxx-abi/abi.html).  We will not repeat that
description here.

There is no compression *per se* in the IA-64 ABI name mangling scheme.
Instead, entities that have appeared earlier in a mangled name can be
referenced again by way of a "substitution," which is essentially the encoding
"``S``" followed by a number (base 36) indicating which of the preceding
entities is repeated.  Entities of certain kinds (types, qualified name
prefixes, templates) are assigned numbers in their order of appearance in the
mangled name, and a later substitution can refer to such an entity by number.
Certain types and templates in the standard ``iostream`` header are assigned
predefined substitution codes so that they can be referenced cheaply.  For the
implementation of substitutions, see ``add_substitution_if_available`` and
``alloc_substitution``.  Note that one consequence of substitutions is that it
is impossible to reuse mangled names for entities as part of a larger mangled
name including that entity, because the substitution numbering in the reused
piece would be wrong for the overall mangled name.

Mangled names for entities local to a function are distinguished by a trailing
"discriminator," a number that serves to distinguish like-named entities within
one function.  Such entities are assigned discriminator numbers in the front
end, in strict order of appearance in the function.  For example, the first
variable "``i``" in a function is assigned discriminator number zero, the next
one, etc.  regardless of block scope nesting.  These discriminator values are
recorded in the symbol table (rather than the IL) and IL lowering retrieves
them and includes them in mangled names.

Similarly, string literals within (certain) functions are numbered so that
mangled names can be generated for them.  The C++ standard requires that if an
``extern inline`` function is implemented by multiple copies, a string literal
that appears in the function has the same address in all copies.  This is
implemented by assigning a sequence number to each string in functions that
might exist in multiple copies.  Name mangling then uses that number to
generate a mangled name for the variable created to hold the string value.

For entities "externalized," i.e., static variables or functions made external
because they must be referenced from multiple compilations (e.g., for exported
templates, or in one-instantiation-per-object mode), name mangling uses an
extension to the ABI spec.  This is an encoding of the form

   ``B`` *length module_id*

where *module_id* is a generated string describing the current compilation,
formed from the source file name and some other bits that help make it unique.
This encoding is used as a prefix on the <name> syntax term and is used to make
a local name unique when it is externalized.

For the Microsoft extension that allows explicit specification of a virtual
function to be overridden, name mangling adds the name of the overridden
virtual function as the encoding ``Q``\ *<nested-name>* following the nested
name for the function, i.e., an *<encoding>* has an alternative of *<function
name>*\ ``Q``\ *<nested-name> <bare-function-type>*.  So, for example, a member
``D::f`` that explicitly overrides ``B::f`` would be encoded as
``_ZN1D1fEQN1B1fEv``.  This is an extension to the ABI spec.

Builtin operations in expressions (i.e., vendor-specific extensions like the
Microsoft __is_base_of) are encoded as a vendor-specific operator ``builtin``\
*XX*, where *XX* is the number of the builtin operation.  Any type operands are
encode as "``TO``" followed by the encoding for the type.

When C++/CLI is enabled, tracking references, ``interior_ptr``\ s, ``pin_ptr``\
s, and handles are mangled using the vendor extended type qualifier:

  *<type>* ::= ``U``\ *<source-name><type>* # vendor extended type qualifier

Tracking references use the encoding "``8__trkref``" for *<source-name>*,
``interior_ptr``\ s are encoded as "``14__interior_ptr``", ``pin_ptr``\ s
as "``9__pin_ptr``", and handles are encoded as "``8__handle``".  C++/CLI
static constructors are encoded as "``C8``" and finalizers are "``D7``".
``__nullptr`` is encoded as "``DN``"; ``__underlying_type`` is encoded as
"``Du``".

C++/CLI operators are generally mangled using the IA-64 ABI vendor extended
operator syntax:

  | *<operator-name>* ::= ``v``\ *<digit><source-name>* # vendor extended
    operator

The unary "``%``" operation (which produces a handle to its operand) uses the
"``9clihandle``" <*source-name*>; the array subscript operation (which has a
variable number of operands) is encoded with a <*source-name*> of
"``12clisubscript``".  Due to the limitation of a single digit in this
encoding, only 8 subscripts can be represented in the mangled name.  The
``::typeid`` operation is mangled as "``9clitypeid``"; ``safe_cast`` is
"``12clisafe_cast``".

The vendor extended operator syntax isn't flexible enough to support ``gcnew``,
so an EDG-specific mangling (patterned after the mangling for ``new``) is used:

  .. list-table::

     * - | ::= ``gc_``\ *<type>*\ ``E``
       - | # gcnew *<type>*
     * - | ::= ``gc_``\ *<type><initializer>*
       - | # gcnew *<typ>e* {init}
     * - | ::= ``gc``\ *<expression>*\*\ ``_``\ *<type>*\  ``E``
       - | # gcnew array\ *<type>*\ (dims)
     * - | ::= ``gc``\ *<expression>*\*\ ``_``\ *<type><initializer>*
       - | # gcnew array\ *<type>*\ (dims) {init}

The IA-64 ABI specification doesn't specify how to mangle function names that
use the GNU multiversion feature.  For these functions, the front end produces
mangled names similar to the names that GNU produces.  More specifically, for
the resolver function and for target-specific functions, a suffix is appended
to the name ("``.resolver``" for resolver functions and a string that contains
a sorted list of the canonicalized ``target`` attributes for target-specific
functions).  The mangling for an ``ifunc`` function is somewhat different: the
original name is mangled, then "``.ifunc``" is appended, then the name is
mangled again (with the signature of the lowered routine type).  Note that
these GNU manglings contain periods which can cause problems in certain back
ends (e.g., C-generating back ends), so a configuration macro,
``REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES``, is available to change the
periods into underscores.  Note also that due to their non-standard nature,
these names cannot be demangled (either with or without the periods).

.. _dynamic-init:

Dynamic Initialization
----------------------

The ``a_dynamic_init`` entry, which describes a dynamic initialization, is used
both in C and C++.  IL lowering rewrites cases that are not valid in C and
leaves the others.  See ``lower_dynamic_init``.  Cases that must be rewritten
are:

* | Initialization of a file-scope variable to a nonconstant value.
  |
  | See ``lower_file_scope_dynamic_inits``.
  |
  | The initialization is rewritten as executable code in a file-scope
    initialization routine (called ``__sti__``\ *module-id*, or ``__tls_init``
    for initialization of objects with ``thread_local`` storage).
  |
  | The existence of the initialization routine is communicated to the
    environment through creation of a file-scope static variable called
    ``__link``:

  .. code:: c++

     void __sti__module_id() {...}
     struct __linkl {
       struct __linkl *next;
       void           (*ctor)();
       void           (*dtor)();
     };
     static struct __linkl __link = {NULL, __sti__module_id, NULL};

  | This is compatible with the cfront ``patch`` step.  That tool or something
    similar to it can find the ``__link`` variable and place it on a list of
    entries to be consulted at program startup and termination.  It is also
    compatible with the alternative cfront ``munch`` tool, which looks for
    routines with names beginning with ``__sti__`` and ``__std__``.  Note that
    the cfront approach allows the specification of a termination routine, but
    that facility is not used by this front end (end-of-program destructions
    are requested individually by calling a runtime routine, e.g.,
    ``__record_needed_destruction`` for the cfront-like ABI).  This ``__link``
    variable is included in the program even if initialization is handled by
    some fancier mechanism (see, for example,
    ``USE_INIT_SECTION_IN_GENERATED_C`` for the C-generating back end), and
    even in the IA-64 ABI, even though it may not be used in those cases.
  |
  | See ``make_code_to_invoke_file_scope_init_routine``.
  |
  | When ``ONE_INSTANTIATION_PER_OBJECT`` is TRUE, each instantiation in a
    compilation is put out as a separate object file.  There is still only one
    IL tree, but it is marked so that a back end can sweep through it several
    times and put out a different "slice" each time, each slice containing one
    instantiated entity (function, variable, or static data member) plus
    exactly the set of other entities needed by that entity.  This is done via
    a set of per-instantiation "needed" flags.  For slices that contain
    variable template or static data member instantiations, the slice may
    require file-scope initialization code.  In these cases, multiple
    file-scope initialization routines and multiple ``__link`` variables are
    generated, each one marked to be included in the proper slice.  Each
    file-scope initialization is placed in the initialization routine for the
    appropriate slice and any initialization for objects with ``thread_local``
    storage are put out in a ``__tls_init_``\ N routine where the N indicates
    the number of the slice).
* | Initialization of a local static variable to a nonconstant value.  The
    initialization is rewritten as executable code, then surrounded by a
    first-time test so that the initialization is done only once:

  .. code:: c++

     void f() {
       static int i = j+k;
     }

  | is translated to (cfront-like ABI):

  .. code:: c++

     static int temp;  // Implicitly initialized to zero
     void f() {
       static int i;
       if (temp == 0) {
         temp = 1;
         i = j+k;
       }
     }

  | In the IA-64 ABI, the guard variable is set after the initialization.  If
    ``IA64_ABI_USE_GUARD_ACQUIRE_RELEASE`` is TRUE, the routines given in the
    ABI spec are called, making the full (and potentially thread-safe) sequence

  .. code:: c++

     static long long temp;  // Implicitly initialized to zero
     void f() {
       static int i;
       if (temp == 0) {
         if (__cxa_guard_acquire(&temp)) {
           i = j+k;
           __cxa_guard_release(&temp);
         }
       }
     }

  | See ``add_first_time_test``.
* | Initialization by calling a routine that returns a class object via a copy
    constructor.  The initialization is rewritten as a call of the routine.
* | Initialization using a constructor.  The initialization is rewritten as a
    call of the constructor.
  |
  | See ``add_constructor_call``.
* | Initialization to a nonconstant aggregate.  The initialization is rewritten
    as initialization to a constant aggregate combined with executable code to
    initialize the nonconstant positions.

  .. code:: c++

     void f() {
       int a[3] = {1, i+1, 3};
     }

  | is rewritten as

  .. code:: c++

     void f() {
       int a[3] = {1, 0, 3};
       a[1] = i+1;
     }

  | See

  * | ``lower_ck_dynamic_init``
  * | ``lower_dynamic_init_aggregate_constant``
  * | ``add_destructor_call``

* | Any dynamic initialization requiring a destructor.  The required destructor
    call will be on a list of destructions to be done at the end of an object
    lifetime.  See :ref:`object-lifetimes`.
  |
  | If the dynamic initialization is a valid C initialization except for the
    requirement to call a destructor, it is left as-is; otherwise, it is
    rewritten as described above.
  |
  | When a global or local static variable is constructed, and the variable
    requires later destruction, this is recorded at runtime by calling the
    routine ``__record_needed_destruction`` [#f2]_ (cfront-like ABI) or
    ``__cxa_atexit`` (IA-64 ABI) The calls of that routine build a list of
    entities to be destroyed at the time of program termination.  If a
    destruction requires only a destructor call, the data structure passed to
    the runtime routine gives the address of the object and the address of the
    destructor.  Otherwise (for more complex cases, e.g., arrays), a routine is
    generated containing the necessary destruction code, and the address of
    that routine is passed in the data structure, along with a NULL object
    pointer indicating the complex case.
  |
  | See ``record_needed_destruction``.

In routines that return a class value by calling a copy constructor, the
``stmk_return`` statement is unusual in that it points to a dynamic
initialization entry that describes the initializing operation to be done to
return a value.  The destination of the initialization is taken to be the
address given by the implicit return value parameter added to the routine.  The
caller passes in the address of a class object, and the code generated for the
return statement initializes that object before returning.

Main Program
------------

A call of the runtime routine ``_main`` is inserted at the start of the main
program.  That routine invokes initialization code for file-scope variables,
and arranges things so that the corresponding termination code will be called
when the program terminates.  See ``lower_scope``, and the section on dynamic
initialization in this chapter.

Class Types
-----------

The following processing is done on class types:

* | If the class has virtual base classes, a copy of the class type is made
    without the virtual base classes.  This copy is given the name of the
    original class prefixed with "``__SO__``".  This type is attached to the
    original type through the ``type_as_subobject`` pointer.  If the class has
    no virtual base classes, the type-as-subobject is the same as the original
    class type.  See ``make_subobject_class_type``.
* | The field list of the class is augmented by adding generated fields for the
    base classes of the class (using the type-as-subobject of the base classes
    for the field types) and pointer to a virtual function table and (for the
    cfront-like ABI) any required pointers to virtual base classes .  This does
    not change the size of the class -- these entities were already part of the
    class; they are just made explicit so they can be referenced as fields.
    See ``prelower_class_type``.
* | If the class has virtual functions, a variable will be created for a
    virtual function table for the class and for each base class of the class
    as a subobject of the class.  The declaration is created as an ``extern``
    declaration, and is turned into a definition (either external or static)
    that is initialized if the virtual function table definition should be put
    out.  (See next section and ``make_vars_for_virtual_function_tables``).
* | If the class has virtual base classes, in the IA-64 ABI, information needed
    to access virtual base classes and to call virtual functions in such bases
    is determined and recorded.  See
    ``compute_vbase_and_vcall_offset_indices``.
* | The members of the class (local constants, local types including nested
    classes, static data members, and member functions) are promoted out of the
    class into the surrounding context.  See ``promote_class_members`` and
    ``do_scope_class_member_promotion``.
* | To support the anachronism that allows a static data member to be declared
    but never defined, the storage class of static data members that are
    externally visible and have no initial value is changed to ``sc_extern``.
    See ``lower_scope``.

Virtual Function Tables
-----------------------

Each class containing virtual functions has associated with it a virtual
function table.  If it has base classes that also contain virtual functions, it
will also have associated with it an instance of each base class's virtual
function table adjusted for use when that base class is a subobject within a
complete object of the derived class.  So, associated with a class ``C`` we
might have a virtual function table for ``C`` as a complete object, one for
``B`` as a subobject of a complete object of type ``C``, and one for ``A`` as a
subobject of a complete object of type ``C``.

In the cfront-like ABI, each of these virtual function tables is embodied in a
variable whose type is an array of entries of type

.. code:: c++

   typedef void (*__vptp)();
   struct __mptr {
     short d;
     short i;
     __vptp f;
   };

``d`` is the "delta" value to be added to the "``this``" pointer to adjust
it to point to the class in which the virtual function is defined; ``f``
points to the virtual function.  For a pure virtual function, the function
pointer points to the runtime routine ``__pure_virtual_called``.  In the
case of a deleted virtual function, a pointer to the runtime routine
``__deleted_virtual_called`` is used for the function pointer.

Note that field ``i`` in the above is not used; it's there for cfront
compatibility.  cfront uses the same data structure for pointers to member
functions and virtual function table entries, even though a simpler structure
could have been used for virtual function tables.

In the IA-64 ABI, each of these virtual function tables is embodied in a
variable whose type is an array of entries of type ``ptrdiff_t``, which is
used to encode function pointers, offsets, etc.  There is no "delta" value.
Entries that require such an adjustment point to a thunk routine, which
adjusts the "``this``" pointer and then calls the proper routine.  For a
pure virtual function, the function pointer points to the runtime routine
``__cxa_pure_virtual``; a deleted virtual function has its function pointer
initialized to the ``__cxa_deleted_virtual`` runtime routine.

Each entry defines one virtual function of the class.  The routine entry for
the virtual function gives the virtual function number for the function, which
serves as the index into the virtual function table.  In the cfront-like ABI,
the code generated uses entry ``[0]`` of the virtual function table to point to
the typeinfo for the class, and the first function is at entry [1].  In the
IA-64 ABI, the first function is at entry [0], but there are also negative
entries: [-1] points to the typeinfo for the class, [-2] gives the offset to
the beginning of the complete object , and [-3], [-4], etc., if needed, give
offsets to virtual base classes and offsets to be added to the "``this``" value
when calling virtual functions.

When an entity of a class type having virtual functions is initialized (by a
constructor), the virtual function table pointer in the class entity is set to
point to the virtual function table that is appropriate for that object (i.e.,
the one that properly reflects whether the class entity is a complete object or
a subobject of some other class).  Note that it's the virtual function pointer
that is changed; the virtual function tables themselves are constant and are
not changed.  They are not built up, on a per-routine basis or otherwise, for a
given entity.  Rather, the proper complete table is chosen and a pointer to it
is put into the class entity.  Many class entities can point to the same
virtual function table.

The variables for virtual function tables are externally visible (except for
classes with internal or no linkage), and they are shared between compilation
units to save space.  For the sharing scheme to work correctly, exactly one
compilation unit must define the contents of each virtual function table.  The
rest of the compilation units must put out the variable as an ``extern``
declaration.  What IL lowering does is to build every variable as an ``extern``
declaration first, then change it to a definition (external or static) if the
virtual function table should be defined in the present compilation.  The
heuristic used to decide whether or not a virtual function table definition
should be put out is the one described in ARM 10.8.1c: The virtual function
table for a class is put out if the compilation contains a definition for the
first non-inline virtual member function of the class.  If the class contains
no non-inline virtual function, the virtual function table is defined as a
local static variable unless the ``--suppress_vtbl`` or ``--force_vtbl`` option
is specified on the command line.  This same heuristic is used for the IA-64
ABI [#f3]_, with the modification that virtual function tables that would be
put out as static are put out as COMDATs, so that the multiple copies can be
joined together by the linker.

To define a virtual function table, the variable is given a static initial
value that's specified by a ``ck_aggregate`` constant.  Each entry in the table
is defined appropriately.  For a complete-object virtual function table, the
functions that go into the table are simply the virtual functions of the class
in the order of their virtual function numbers.  For a virtual function table
for a subobject of class ``A`` in a complete object of class ``B``, the member
functions of ``A`` are scanned looking for virtual functions, while at the same
time the override list for base class ``A`` of ``B`` is scanned.  If a function
in ``A`` does not appear on the override list, the function from ``A`` goes
into the virtual function table.  Otherwise, the function from the override
list goes into the virtual function table.

During the execution of constructors and destructors for base class subobjects,
virtual functions should act as if the complete object has the type of the
constructor or destructor class.  That is, functions in classes further out
than the subobject class do not override during the subobject construction or
destruction.  When virtual base classes are involved, this means that some
virtual function table instances need to be built to reflect three classes: a
class A (whose virtual function table this is) that is a subobject of a class B
(whose constructor is executing, and which therefore dictates overriding) that
is a subobject of a class C (which is the complete object type, and which
therefore dictates layout).  This is necessary when there are virtual base
classes, and not otherwise, because the "delta" field in the virtual function
table is based on the offset between the objects for the classes of the
overridden and overriding functions, and that offset varies depending on the
type of the complete object.

In the IA-64 ABI, the normal (non-construction) virtual function table variable
for a class contains the primary virtual function table plus the virtual
function tables for the base classes of an object of that type, all
concatenated end-to-end in the single array.  This is as required by the ABI
specification.  For construction vtables, however, where the spec makes no such
requirement, each vtable is in a separate variable.  In the cfront-like ABI,
every vtable is in a separate variable.

See

* | ``make_vars_for_virtual_function_tables``
* | ``make_var_for_virtual_function_table``
* | ``define_one_virtual_function_table``
* | ``define_virtual_function_tables``
* | ``define_scope_virtual_function_tables``
* | ``add_vtbl_entry_init``
* | ``virtual_function_table_should_be_defined_here``
* | ``make_construction_vtbls``

Covariant return types on virtual functions
-------------------------------------------

An overriding virtual function can return a pointer or reference to a
derived class where the overridden function returns a pointer or reference
to a base class.  Such an overriding function is said to be *covariant*
with the overridden function.  This requires special handling in IL
lowering because a call that appears (statically) to be to the overridden
function may in fact (dynamically) be a call to the overriding function,
and the value returned from the overriding function must undergo a
derived-to-base adjustment before it is returned to the caller.  This is
implemented by adding wrapper functions: for each combination of an
overriding function and an overridden function, a wrapper is generated that
calls the overriding function, casts the returned value to the proper type,
and returns it.  The virtual function table points to the wrapper function
from slots that need to call the overriding function but need the interface
of the overridden function.

This mechanism is also used more generally in the IA-64 ABI, for thunks
required for virtual function calls where a "``this``" adjustment is needed.

The IL representation for a wrapper function is a routine with

* | ``overriding_function_for_wrapper`` and
* | ``overridden_function_for_wrapper``

set appropriately.  In the case where a wrapper is being used for a covariant
return type adjustment, the wrapper body is simply a return of an
``enk_result_of_overriding_function`` node cast to the proper
pointer-to-base-class.  The ``enk_result_of_overriding_function`` node
represents the value returned from calling the overriding function with the
arguments passed to the wrapper.

This is controlled by the macro
``ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN``.

While the IL representation is a wrapper function, the underlying
implementation need not be.  In fact, for functions with variable-length
argument lists, a wrapper approach may not be feasible.  Instead, a back end
can turn the wrapper routines into entry points of the overriding function, or
duplicate the body of the overriding function within each wrapper routine.

Namespaces
----------

The processing for namespaces is simply the promotion of the namespace members
out of the namespaces, with appropriate name mangling.  See
``do_namespace_member_promotion``.

Routines
--------

The lowering for routines consists of making two kinds of implicit parameters
explicit:

* | The implicit "``this``" parameter.  This is present on nonstatic member
    functions.
* | The implicit parameter that provides the address into which a class value
    should be returned, for functions returning a class by value.  This
    mechanism is not used for simple C-style structures, but is needed for more
    complicated classes, because for those it must be possible to take the
    address of a class value even if it is an rvalue, e.g., to cast to a base
    class or call a member function.  In this more complicated calling scheme,
    the caller passes in the address of a temporary and the called routine
    places the class value there on return.

Both the ``param_type_list`` in the routine type and the parameter variable
entries in the routine scope are updated to make the implicit parameters
explicit.

Parameters that are classes passed by value and requiring a copy constructor
require additional processing: Since classes that require a copy constructor
cannot be copied arbitrarily and cannot be moved, the caller actually does the
copy constructor call into a temporary and passes the address of the temporary
as the argument.  The parameters on the receiving end must be changed to be
pointers to the class object, and all the references to the parameters must be
changed to add the extra indirection.

See ``lower_type`` and ``lower_scope``.

Promotion of Local Entities to File Scope
-----------------------------------------

If the flag ``PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE`` is TRUE, local static
variables and local types of functions will be promoted out of those functions
into the file scope under some conditions.  Such promotion is enabled when the
IL will be fed into the C-generating back end, and may be desirable in other
cases where the output is textual and block-structured in some way.

The problem addressed by this transformation is shown by the following example:

.. code:: c++

   void f() {
     static int i;
     class A {
       void g() {
         i = 1;       // Reference to variable local to function f
       }
     };
   }

The body of ``A::g`` will become a free-standing function at file scope and yet
will have to reference the variable ``i`` of ``f``.  Therefore ``i`` must be
promoted out of ``f`` into the file scope so that it will be put out in the
file scope in the generated C code.

This is not a problem for an ordinary back end, as the reference to ``i`` will
be made in object code form or within the flat name space of an
assembly-language program.

See

* | ``local_entities_should_be_promoted``
* | ``promote_local_entities_to_file_scope``

.. _ctor-wrapper:

Constructor Wrapper Code
------------------------

Constructors are modified to add the "wrapper" code that is required.

The constructor-initializer list indicates all the initializations that must be
done for base classes and members.  It is in the language-defined order for
initialization: virtual base classes, then non-virtual base classes, then
members.  Each base class or member that requires initialization will appear on
the list, with a front-end-generated default initialization if there is no
user-written one in the source.

The modifications made are:

* | In the cfront-like ABI, if the class has virtual base classes, a parameter
    is added for each such class.  These parameters are used to pass in the
    addresses of the virtual base classes when the constructor is called (from
    another constructor) to initialize a subobject; when the constructor is
    called to initialize a complete object, the parameters are passed as NULL.
    In the IA-64 ABI, a single parameter is always added, for the VTT (virtual
    table table) pointer passed in.  Again, NULL is passed in to indicate a
    complete object.
* | In the cfront-like ABI, code is added to set any virtual base class
    pointers in the class.  The virtual base class pointers in the base classes
    are set by the constructor calls for the base classes and therefore need
    not be set by code at this level.
* | Code is added to call a constructor for each virtual base class that
    appears on the constructor-initializer list if a complete object is being
    initialized.
* | Code is added to call a constructor for each non-virtual base class that
    appears on the constructor-initializer list.
* | Code is added to initialize each member that appears on the
    constructor-initializer list.
* | If the class has any virtual functions, code is added to set the virtual
    function table pointers in the current class and all of its base classes
    that have virtual functions.
* | If allocation can be folded into the constructor (a configuration
    option, but always true if assignment to "``this``" is supported), the
    entire routine is surrounded by code that tests the incoming "``this``"
    parameter, and if it is NULL calls an appropriate ``new`` routine to
    allocate space, which is then initialized.  If the constructor actually
    contains an assignment to "``this``", the ``new`` code is not inserted and
    the wrapper code above is inserted after each assignment to "``this``"
    instead of at the beginning of the routine.

If the class has any virtual functions that are overridden in such a way that
there is a virtual base class step between the class of the overridden function
and the class of the overriding function, special virtual function instances
are required during construction of the class as a subobject.  A constructor
for such a class, when called for a subobject, expects to receive an array of
virtual function table pointers from its caller (the constructor for the
derived class).  It uses these virtual function table instances when setting
virtual function table pointers, and it passes parts of the array to its own
subobject constructors.  In the cfront-like ABI, the pointer to the array is
passed (and received, in the subobject constructor) by setting the so-called
"transfer pointer" (a virtual function table or virtual base class pointer in
the class object that is unset at that point and can be borrowed for this
purpose).  In the IA-64 ABI, the pointer to the array is passed in via the
added VTT pointer parameter.

In the IA-64 ABI, constructors have multiple entry points.  The entry for a
complete object has a "``C1``" encoding in its mangled name, and has no added
parameters.  The entry for a subobject has "``C2``", and has an added VTT
pointer parameter if the class has virtual bases.  The IL lowering
implementation puts the real code for the constructor in one function, which is
the subobject constructor if the class has virtual bases and the complete
object constructor otherwise.  The other constructor is referred to as the
alternate entry point.  When the class has virtual bases and
``HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS`` is TRUE, the construction of
virtual bases is performed in the complete constructor before invoking the
subobject constructor; otherwise the alternate entry is simply an entry point
that calls the primary routine.  Calls from an entry point to the primary
routine can be inlined if appropriate, or a back end can implement the entry
point functions as actual entry points to a single routine.  See
``alternate_entry_point``.

See

* | ``lower_constructor_code``
* | ``add_constructor_params``
* | ``add_constructor_wrapper_code``
* | ``lower_ctor_init``

Destructor Wrapper Code
-----------------------

Destructors are modified to add the "wrapper" code that is required.

The constructor-initializer list indicates all the destructions that must be
done for base classes and members.  It is in the language-defined order for
destruction, i.e., the reverse of the initialization order.  Each base class or
member that requires destruction will appear on the list, with a
front-end-generated destruction (there is no way to explicitly write one in the
source program).

The modifications made are:

* | A parameter of type ``int`` is added.  This parameter is used by the caller
    as a bit mask: the ``0x2`` bit of the parameter set to ``1`` indicates a
    complete object, and the ``0x1`` bit set to ``1`` indicates that the
    storage for the object should be freed.
* | If the class has any virtual functions, code is added to set the virtual
    function table pointer in the class and in all of its base classes, so that
    virtual function calls executed during the destruction will not call
    functions in further-derived classes.

The above code is added at the beginning of the routine.  The rest of the
wrapper code is added at the end of the routine.  To make sure the code at the
end is reached, all ``return``\ s in the routine body are rewritten as
``goto``\ s to the epilogue code.

* | Code is added to destroy each member that appears on the
    constructor-initializer list.
* | Code is added to destroy each non-virtual base class that appears on the
    constructor-initializer list.
* | Code is added to destroy each virtual base class that appears on the
    constructor-initializer list if a complete object is being destroyed.
* | A ``delete`` routine is called to free the storage if the proper bit in
    the implicit parameter is set.  The entire routine is also enclosed in
    an ``if`` statement so it will do nothing if the "``this``" parameter
    passed in is NULL.  If an assignment to "``this``" was actually done in the
    routine, this ``delete`` is protected by a test to see if the "``this``"
    parameter is (still) non-NULL.

See :ref:`ctor-wrapper` regarding special virtual function tables used
during subobject construction.  Similar processing is also needed in
destructor wrappers for subobject destruction.

In the IA-64 ABI, destructors have multiple entry points.  The entry for a
complete object has a "``D1``" encoding in its mangled name, and has no added
parameters.  The entry for a subobject has "``D2``", and has an added VTT
pointer parameter if the class has virtual bases.  The entry to destroy and
free storage for a complete object has "``D0``" and no added parameters.  The
IL lowering implementation puts the real code for the destructor in one
function, which is the subobject destructor if the class has virtual bases and
the complete object destructor otherwise.  The other destructors are known as
alternate entry points.  When the class has virtual bases and
``HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS`` is TRUE, the destruction of
virtual bases is performed in the complete destructor after invoking the
subobject destructor; otherwise alternate entry points simply call the primary
routine (for the deleting destructor, the entry point also contains the
deallocation code).  Calls from an entry point to the primary routine can be
inlined if appropriate, or a back end can implement the entry point functions
as actual entry points to a single routine.  See ``alternate_entry_point``.

See

* | ``lower_destructor_code``
* | ``add_destructor_params``
* | ``lower_dtor_init``
* | ``lower_destructor_dynamic_init``

Calls
-----

See ``lower_call``.

Calls of constructors and destructors are altered to add the implicit
parameters described in the previous sections.  Except for constructor and
destructor calls in wrapper code, constructor and destructor calls are for
complete objects.

See ``make_ctor_implied_arg_list`` and ``make_dtor_implied_arg_list``.

Calls of "vacuous" destructors, as in

.. code:: c++

   p->int::~int();

are changed from ``eok_vacuous_destructor_call`` or
``eok_points_to_vacuous_destructor_call`` operations to a cast to ``void``.

Virtual function calls are rewritten from

.. code:: c++

   object->func(args ...)

to (cfront-like ABI)

.. code:: c++

   ((vtbl_temp = (object->__vptr)+index),
     (object+vtbl_temp->d)->(vtbl_temp->f)(args ...))

(Omitting some casting for clarity.) In other words, ``vtbl_temp`` is
assigned the address of the proper virtual function table entry, the
"``this``" pointer is adjusted by the "delta" value in the entry, and the
function indicated in the entry is called.  If ``object`` is not a reusable
expression, the first occurrence of ``object`` above is replaced by
``(object_temp = object)``, and the second by ``object_temp``.

For the IA-64 ABI, the code is

.. code:: c++

   (object->__vptr[index])(object, additional_args ...)

(recall that any required "``this``" adjustment is handled by thunk
routines and therefore is not needed at the call site).

See ``lower_virtual_function_call``.

A related case (though not a call) is lowering of ``eok_virtual_function_ptr``,
the operator that implements the anachronism of casting a bound member function
pointer to a normal function pointer.  It rewrites

.. code:: c++

   (func-type *)(object->func)

to (cfront-like ABI)

.. code:: c++

   (func-type *)((object->__vptr)+index)->f)

or (IA-64 ABI)

.. code:: c++

   (func-type *)*(object->__vptr)+index

See ``lower_virtual_function_ptr``.

Return Value Optimization
-------------------------

Return value optimization applies to functions that return a value via a copy
constructor.  It can be done when all ``return`` statements in the function
return the same nonstatic local variable.  The optimization is to rewrite all
references to the local variable to refer instead to the return-value address
passed in by the caller, thus avoiding a copy constructor call on exit.  For
example:

.. code:: c++

     struct A {
       A(int);
       A(const A&);
     };
     A f() {
       A a(1);       // Constructed into temp passed by caller
       return a;     // No copy needed
     }

The front end proper detects the possibility of doing the optimization, but the
transformation is done by IL lowering.  There are several parts to the
optimization:

* | References to the address of the local variable are rewritten as references
    to the (pointer) value of the return-value address parameter.
* | The local variable is marked as unreferenced, and any initialization on it
    is turned into initialization of the object pointed to by the return-value
    address parameter.
* | Return statements are transformed into simple returns doing no copy
    constructor call and returning no value.

Expression Temporaries
----------------------

``enk_temp_init`` nodes are used to introduce class temporaries in the middle
of expressions.  They indicate the initialization to be done on the temporary
and any destruction to be done once the temporary is no longer needed.  The
temporary is implicit in the high-level IL; IL lowering allocates a variable
for the temporary and uses it in place of references to the ``enk_temp_init``
node.

The initialization and destruction are indicated by a dynamic initialization
entry, which is rewritten in the usual way.  See ``lower_expr_full``.

One special problem: When class temporaries created under a conditional
operator ("``?``", "``&&``", or "``||``") require destruction, the destruction
must be done only if the initialization was done.  For each case like this, it
is necessary to create a temporary, initialize it to zero at the start of the
block containing the expression, set it to one if the initialization is done,
and test it on exit from the block to determine whether or not to do the
destruction.  See ``add_dyn_init_cleanup, init_conditional_flag_var,
set_conditional_flag_var, add_conditional_flag_test``.

New and Delete
--------------

``new`` and ``delete`` are represented as an ``enk_new_delete`` expression
node.

There are three main ``new`` cases (see ``lower_new``):

* | A simple ``new`` is translated to a call of the appropriate ``new``
    routine.  If the allocated object must be initialized, the pointer returned
    from the allocation is tested to ensure that it is non-NULL, and then the
    initialization is done.
* | A simple class ``new`` is often translated to a constructor call, passing a
    NULL pointer as an indication that the constructor should do the
    allocation.  (This is enabled by ``NEW_CAN_BE_FOLDED_INTO_CTOR``.  When
    that is FALSE, the new routine is called, and if the allocation succeeds
    the constructor is called.)
* | An array ``new`` is translated into a call of a runtime routine; see below.

There are three main ``delete`` cases (see ``lower_delete``):

* | A simple ``delete`` is translated into a call of the appropriate ``delete``
    routine.  If the allocated object must be destroyed, the destructor is
    called before the deallocation routine.
* | A simple class ``delete`` is usually translated into a destructor call,
    passing a flag indicating that the destructor should do the deallocation.
* | An array ``delete`` is translated into a call of a runtime routine; see
    below.

Calls of ``new`` and ``delete`` that allocate or free arrays of classes that
have constructors or destructors are specially handled.  They are converted to
calls of:

  | cfront-like ABI:

    | ``__vec_new(``\ *array, num_elems, elem_size, ctor_routine*\ ``)``
    | ``__vec_delete(``\ *array, num_elems, elem_size, dtor_routine,
      free,* ``0)``

  | IA-64 ABI:

     | ``__cxa_vec_new(``\ *num_elems, elem_size, padding,
       ctor_routine, dtor_routine*\ ``)``
     | ``__cxa_vec_delete(``\ *array, elem_size, padding,
       dtor_routine*\ ``)``

where

.. list-table::

   * - | *array*
     - | is the address of the first element of the array.  For
         ``__vec_new``, it may be ``NULL`` to indicate that the space for the
         array should be allocated.  For ``__vec_delete``, it may be ``NULL``
         to indicate that the routine should do nothing.
   * - | *num_elems*
     - | is the number of elements in the array.  For
         ``__vec_delete``, it may be ``-1`` to indicate that the number of
         elements given in the corresponding ``__vec_new`` should be used.
   * - | *elem_size*
     - | is the size in bytes of each array element.
   * - | *ctor_routine* or *dtor_routine*
     - | is the address of the constructor or destructor routine to call, or
         NULL if no routine should be called.
   * - | *free*
     - | is non-zero to indicate that the storage for the array should
         be freed.
   * - | *padding*
     - | is the size of the array prefix in which the number of elements is
         recorded by the runtime.

The final argument of the ``__vec_delete`` call is not used; it is present for
compatibility with cfront.

These routines are necessary in order to save the number of elements in the
array from the time of the ``new`` (where it is known) to the time of the
``delete`` (where it is not available), and also to call the constructors and
destructors for all the array elements.

For the cfront-like ABI, when exception handling is enabled, and only for
classes that have destructors, ``__vec_new_eh`` is called instead of
``__vec_new``.  It has an additional final parameter that is a pointer to the
destructor.  This is used to destroy any constructed elements if an exception
is thrown while ``__vec_new_eh`` is initializing the array elements.

For the cfront-like ABI, when value-initialization is to be done for a class
that has a generated constructor, ``__vec_new_eh_zero`` is called.  It has the
same parameter list as ``__vec_new_eh``, but it zeroes the storage before
calling the constructor.

With array new and delete, it is possible to define class-specific

* | ``operator new[]`` and
* | ``operator delete[]``

functions. When those are specified, different runtime routines are called:

  | cfront-like ABI:

    | ``__array_new(``\ *num_elems, elem_size, ctor_routine,*
    | |_| |_| |_| |_| |.| |.|
      *dtor_routine, new_routine, delete_routine, is_two_arg*\ ``)``
    | ``__array_delete(``\ *entity_node, num_elems, elem_size, dtor_routine,*
    | |_| |_| |_| |_| |_| |_| |.|
      *delete_routine, is_two_arg*\ ``)``

  | IA-64 ABI:

    | ``__cxa_vec_new2(``\ *num_elems, elem_size, padding, ctor_routine,*
    | |_| |_| |_| |_| |_| |_| |.|
      *dtor_routine, new_routine, delete_routine*\ ``)``
    | ``__cxa_vec_new3(``\ *num_elems, elem_size, padding, ctor_routine,*
    | |_| |_| |_| |_| |_| |_| |.|
      *dtor_routine, new_routine, delete_routine*\ ``)``
    | ``__cxa_vec_delete(``\ *array, elem_size, padding, dtor_routine*\ ``)``

Most of the parameters are as described above. The others are:

.. list-table::

   * - | *new_routine*
     - | is the address of the special ``operator new[]``
         routine, or NULL if the default routine should be used.
   * - | *delete_routine*
     - | is the address of the special ``operator delete[]``
         routine, or NULL if the default routine should be used.  Also NULL on
         ``__array_new`` if exceptions are disabled.
   * - | *is_two_arg*
     - | is ``1`` to indicate that the ``delete_routine`` takes
         two arguments, ``0`` otherwise.

These routines are used only if ``ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE`` is
TRUE.  For the cfront-like ABI, if value-initialization is to be done,
``__array_new_zero`` is called in place of ``__array_new``.  The IA-64 ABI
"``new3``" routine is used for the two-argument delete case.

For the cfront-like ABI, when ``ABI_CHANGES_FOR_PLACEMENT_DELETE`` is TRUE, it
is possible to do a placement new of an array type and later deallocate the
storage via the ``delete`` operator.  For that case, a special runtime routine
is called to ensure that the array size is recorded:

   | ``__placement_array_new(``\ *entity_node, num_elems, size_elem,*
     *ctor_routine, dtor_routine*\ ``)``

and the value of the global runtime variable ``__array_new_prefix_size`` is
added to the size of the allocation requested in the placement new to allocate
extra space to contain the array prefix.  See
``make_placement_array_new_call``.  If value-initialization is to be done,
``__placement_array_new_zero`` is called in place of ``__placement_array_new``.

See ``lower_array_new`` and ``lower_array_delete``.

Constructors and Destructors for Arrays
---------------------------------------

Construction or destruction of an entire (static or automatic) array is done by
calling runtime routines.  For the cfront-like ABI, ``__vec_new`` or
``__vec_delete`` are used (see previous section).  The address of the array is
passed into ``__vec_new``, thus suppressing allocation, and ``0`` for the flag
``free`` is passed into ``__vec_delete``, thus suppressing freeing of the
storage.  For the IA-64 ABI, there are specific runtime routines for these
functions:

  | ``__cxa_vec_ctor(``\ *array, num_elems, elem_size, ctor_routine,*
    *dtor_routine*\ ``)``
  | ``__cxa_vec_dtor(``\ *array, num_elems, elem_size, dtor_routine*\ ``)``

Copying of an entire array using a copy constructor is done in the cfront-like
ABI by calling ``__vec_cctor``, passing the array address, the number of
elements, the element size, the address of the routine to call, and the source
array address.  [#f4]_ In the IA-64 ABI, the runtime routine is
``__cxa_vec_cctor.``

If the constructor involved has default arguments, a wrapper routine is created
that requires no arguments other than the "``this``" parameter and calls the
constructor with the proper default arguments.  The wrapper routine is then
passed to the runtime routine, which can call it like a normal constructor.

See

* | ``add_array_constructor_call``
* | ``add_destructor_call``
* | ``default_version_of_routine``
* | ``make_vec_delete_call``

Pointers to Members
-------------------

Pointer-to-member types (``tk_ptr_to_member``) are transformed as follows:

* | Pointers to data members are converted to values of an integral type.  The
    integer contains a byte offset to the data member in the class.  In the
    cfront-like ABI, one is added to leave the zero value free to indicate a
    NULL pointer to data member.  In the IA-64 ABI, nothing is added, and -1 is
    used for a NULL pointer to data member.

  .. code:: c++

     struct A { int i; int j;};
     int A::*pdm = &A::j;       // unsigned int pdm = 5(cfront) 4(IA-64)
     int A::*pdmn = 0;          // unsigned int pdmn = 0(cfront) -1(IA-64)

* | Pointers to member functions are represented by structures with the form

  * | cfront-like ABI:

  .. code:: c++

     typedef void (*__vptp)();
     struct __mptr { short d; short i; __vptp f; };

  | This is essentially the structure described in 8.1.2.c of the ARM.
    ``d`` is the offset to be added to the "``this``" pointer; ``i`` is the
    index into the virtual function table (or -1 for a nonvirtual function,
    or 0 for a NULL pointer); ``f`` is the nonvirtual function pointer when
    ``i`` is -1 or the offset to the virtual function table pointer
    (appropriately cast) when ``i`` is greater than zero.

  .. code:: c++

      struct A {int f(); virtual int g();};
      int (A::*pmf)() = &A::f;       // struct __mptr pmf = {0, -1, &f__1AFv};
      int (A::*pmg)() = &A::g;       // struct __mptr pmg = {0,  1, 0};

  * | IA-64 ABI:

  .. code:: c++

      typedef void (*__vptp)();
      struct __mptr { __vptp f; ptrdiff_t d; };

  | ``f`` is zero for a NULL pointer, is a pointer to a function for the
    non-virtual case, and is the vtable offset in bytes plus one for the
    virtual function case.  ``d`` is the offset to be added to the "``this``"
    pointer.  When ``IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR`` is
    TRUE, an alternate representation is used that works on architectures where
    the low-order bit of a function address can be non-zero.  In that
    representation, ``f`` is zero for a NULL pointer (but you also have to
    check that the low-order bit of ``d`` is zero), is a pointer to a function
    for the non-virtual case, and is the vtable offset in bytes for the virtual
    function case.  ``d`` is the offset to be added to the "``this``" pointer,
    shifted left one bit, and its low-order bit is one for the virtual function
    case.

See ``lower_type``.

Pointer-to-member constants (``ck_ptr_to_member``) are transformed to the above
forms.  See ``lower_ptr_to_member_constant``.  The function case generates a
``ck_aggregate`` constant.  Such constants can only be used when the constant
appears as an initializer value; for other kinds of references (e.g., in an
assignment statement), the reference to the constant is changed to a reference
to a temporary variable that is initialized with the aggregate constant.

Assignment and comparison of pointers to data members are changed to integer
assignment and comparison operators.  Assignment of pointers to member
functions is changed to structure assignment.  Comparison of pointers to member
functions is changed to

  | cfront-like ABI:

    .. code:: c++

     (op1.i == op2.i) && (op1.i == 0 || (op1.d == op2.d && op1.f == op2.f))

  IA-64 ABI:

    .. code:: c++

     (op1.f == op2.f) && (op1.f == 0 || op1.d == op2.d)

  IA-64 ABI alternate representation:

    .. code:: c++

     (op1.f == op2.f) && ((op1.f == 0 && (((op1.d | op2.d) & 1) == 0))
                        || op1.d == op2.d)

(That is the code for ``==``; the code for ``!=`` is similar.) If one of the
operands is a constant, the comparisons are done directly against the constant
values of the components of the constant, and unnecessary parts of the
expression above are omitted.  A comparison against a null pointer to member
constant, for example, becomes simply

.. code:: c++

   (op1.i == 0)

for the cfront-like ABI.

Casting a pointer to a data member to a base or derived class is changed to

  | cfront-like ABI:

  .. code:: c++

   (pdm != 0) ? (pdm + offset) : 0

  IA-64 ABI:

  .. code:: c++

   (pdm != -1) ? (pdm + offset) : -1

Casting a pointer to a member function to a base or derived class is changed to

  | cfront-like ABI:

  .. code:: c++

   (temp = pmf, (temp.i != 0) ? (temp.d += offset) : 0, temp)

  IA-64 ABI:

  .. code:: c++

   (temp = pmf, (temp.f != 0) ? (temp.d += offset) : 0, temp)

  IA-64 ABI alternate representation:

  .. code:: c++

   (temp = pmf, (temp.f != 0 || temp.d != 0)
                              ? (temp.d += offset) : 0, temp)

The tricky issue in both cases is preserving a NULL pointer to member.

See ``lower_pm_related_class_cast``.

Casts from one pointer-to-member-function type to another (not related) are
removed since both types will now have the same representation and since the
cast, left in, would be a cast to a structure type.  Similar casts between
pointers to data members are casts from an integral type to the same integral
type and are left alone.

The ``eok_pm_points_to_field`` and\ ``eok_pm_field`` operators, which implement
the ``->*`` and ``.*`` operators for pointers to data members, are rewritten:

.. code:: c++

   p->*pdm

is lowered to

.. code:: c++

   (member-type *)(((char *)p)+(pdm-1))

The IA-64 ABI version does not need the -1.

See ``lower_pm_field``.

The ``eok_pm_call`` operator, which implements calls of functions identified by
a pointer to member, is rewritten:

.. code:: c++

   object->pmf(args ...)

is lowered to (cfront-like ABI)

.. code:: c++

   ((this_temp = (object_type *)((char *)object + pmf.d)),
                           // Adjust "this" pointer by delta from
                           //   pointer-to-member.
    (func_temp = (function_type *)
                           // The computed function address gets cast to the
                           //   proper function type.
       ((pmf.i < 0) ?      // Check virtual/non-virtual pointer-to-member.
         pmf.f :           // Function address for non-virtual case.
                           // Virtual function case:
         ((vtbl_temp =     // Virtual function table entry address is address
                           //   of virtual function table + offset in table.
            *(__vtbl_entry **)((char *)this_temp +
                               (short)(pmf.f)) +
                           // Pointer to virtual function table is at offset
                           //   pmf.f in the object.
            pmf.i),        // + offset into table
          this_temp = (object_type *)
                        ((char *)this_temp + vtbl_temp->d),
                           // Adjust "this" pointer to get pointer to subobject
                           //   expected by the virtual function.
          vtbl_temp->f))), // Address of virtual function.
    func_temp(             // Call using the function pointer computed.
                           // Arguments for the call:
      this_temp,           //   "this" pointer.
      args ...))           //   Additional arguments, if any.

If ``pmf`` is not a reusable expression, the first occurrence of ``pmf`` above
is replaced by ``(pmf_temp = pmf)``, and the rest by ``pmf_temp``.

If the class and its base classes have no virtual functions, and the variable
``pointer_to_member_call_optimization_allowed`` is TRUE (which does not conform
to the C++ standard), the following simpler version is used:

.. code:: c++

   ((this_temp = (object_type *)((char *)object + pmf.d)),
    eok_call((function_type *)pmf.f,
              this_temp,
              additional_args ...))

This seems slightly more complicated than is needed, but it makes sure that if
a reusable copy of ``pmf`` is needed, the temp for it is initialized before the
call is begun.

For the IA-64 ABI, the code is:

.. code:: c++

   ((this_temp = (object_type *)((char *)object + pmf.d)),
                           // Adjust "this" pointer by delta
                           //   from pointer-to-member.
    (func_temp = (function_type *)
                           // The computed function address gets cast to the
                           //   proper function type.
      ((pmf.f & 1) == 0) ? // Check virtual/non-virtual pointer-to-member.
         pmf.f :           // Function address for non-virtual case.
                           // Virtual function case:
         ((vtbl_temp =     // Virtual function table entry address is address
                           //   of virtual function table + offset in table.
            *(__vtbl_entry **)
              ((char *)this_temp +
                           // Pointer to virtual function table is at offset
                           //   zero in the object
               ((ptrdiff_t)pmf.f - 1)),
                           // + offset into table, dropping low-order bit.
          *vtbl_temp))),   // Address of virtual function.
    func_temp(             // Call using the function pointer computed.
                           // Arguments for the call:
      this_temp,
              //   "this" pointer.
      additional_args ...)) //   Additional arguments, if any.

See ``lower_pm_call``.

Operations Returning Lvalues
----------------------------

Operations that return lvalues in C++ but not in C, i.e., assignment operators,
prefix ``++`` and ``--``, and "``?``" and "``,``" operators, are rewritten in C
form.  Often this involves duplication of expressions.  This rewriting
operation is enabled or disabled by ``LOWER_LVALUE_RETURNING_OPERATIONS``.

Lvalue-returning assignments are rewritten from

.. code:: c++

   (c1 = c2)

to

.. code:: c++

   ((c1 = c2), rcc2)

where ``rcc2`` is a reusable copy of ``c2`` (see
``make_lvalue_reusable_copy``).  The comma operator is an lvalue-returning
operator, which is then further rewritten (see following).

Lvalue-returning "``?``" operators are rewritten from

.. code:: c++

   ((g1 ? g2 : g3) = c2)

to

.. code:: c++

   (g1 ? (g2 = c2) : (g3 = c2))

Lvalue-returning "``,``" operators are rewritten from

.. code:: c++

   ((g1 , g2) = c2)

to

.. code:: c++

   (g1 , (g2 = c2))

For both the "``?``" and the "``,``" cases, the operations in the rewritten
form are lvalue operations iff the "``=``" operation in the original is an
lvalue operation.  The operation indicated as "``=``" can in fact be any
operation (e.g., simple or complex assignment, prefix ``++`` or ``--``, field
selection).  ``c2`` isn't present for unary operations.

Related Class Casts
-------------------

Base or derived class casts are lowered by replacing the cast with an operation
that adds or subtracts the byte offset required.  A few issues require special
handling:

* | If the offset is zero, which it always is with single inheritance, the cast
    can remain a cast between two pointer types.
* | A NULL pointer must pass through a related class cast unaltered.  To do
    that, a conditional test is required around the code that adds or subtracts
    the offset.  To avoid putting out this code in too many places, it is put
    only once around a sequence of related class casts, and it is suppressed
    altogether if (a) all the casts in the sequence have zero offsets, or (b)
    the expression is known to be non-NULL (e.g., the "this" pointer in a
    member function).
* | Casts to virtual base classes involve fetching the virtual base class
    pointer rather than adding or subtracting an offset.

See ``lower_related_class_cast``.

.. _il-bool-type:

bool
----

In modern C++, there is a ``bool`` type, and operations like "``!=``" return
``bool`` instead of ``int``.  (C99 has a ``_Bool`` type, but the boolean
operation -- like "``!=``" and "``>``" -- produce ``int`` values.) The IL
representation of a boolean type is identical to an ordinary small integer
type, except that the variant field ``variant.integer.bool_type`` is TRUE.
That representation is therefore not lowered: A back end can just ignore the
``bool_type`` field and treat a boolean type as the underlying integer type.

Several ``bool`` operations are rewritten by IL lowering:

* | ``eok_bool_cast``, which represents a cast to ``bool`` (or ``_Bool``), is
    rewritten as a "``!= 0``" test of the appropriate type.  See
    ``lower_bool_cast``.
* | An increment of a ``bool`` lvalue is rewritten to set the lvalue to 1
    (``true``).  In the postincrement case, this involves saving the original
    value, setting the lvalue to 1, and then returning the saved original
    value.  See ``lower_bool_increment``.
* | The return types of operations that yield a ``bool`` in C++ but an ``int``
    in C, e.g., "``!=``" and "``&&``", are rewritten to yield an ``int``.  In
    contexts where the result is tested directly, the type is simply changed;
    where something else is done with the result (e.g., storing it in a
    variable), a cast to the proper type is inserted.  See
    ``adjust_bool_operation_types``.
* | Compound assignment operators whose left-hand operand has type ``bool`` (or
    ``_Bool``) are rewritten to ensure that the result value is restricted to
    0/1.  See ``rewrite_compound_assignment``.

Condition Declarations
----------------------

In ``if``, ``while``, ``for``, and ``switch`` statements, the expression tested
can be a condition declaration.  For example:

.. code:: c++

   if (float x = f()) { ... }

This is represented in the IL as an expression node of kind ``enk_condition``.

Loops and non-loops are handled somewhat differently by IL lowering.  Non-loops
(``if``, ``switch``) are rewritten as follows:

.. code:: c++

   if (A x = y) { ... }

is transformed into

.. code:: c++

   {  A x = y;  if (x) { ... }}

The test in the ``if`` actually uses the expression from the ``expr`` field of
the condition, which is the value of the declared variable converted to a
testable type if necessary.

Loops (``while``, ``for``) are rewritten as follows:

.. code:: c++

   for (w; A x = y; z) { ... }

or

.. code:: c++

    while (A x = y) { ... }

is transformed into

.. code:: c++

   {                                    {
      for (w;;) {                          while (1) {
        A x = y;                             A x = y;
        if (!x) {                            if (!x) {
          destroy x if necessary;              destroy x if necessary;
          goto break_label;                    goto break_label;
        }                                    }
        { ... } // Original statement        { ... } // Original statement
        z;      // Increment code
        destroy x if necessary;              destroy x if necessary;
     }                                    }
     break_label:;                        break_label:;
   }                                   }

Again, the ``x`` in ``!x`` is the value of the declared variable converted to a
testable type if necessary.  If the ``break_label`` exists already, it and the
block around the for/while are not added.

See ``lower_condition``.

Anonymous Unions
----------------

In the unlowered IL, field selections of fields of anonymous unions that are
embedded in classes elide the field selection(s) for the anonymous union(s).
For example:

.. code:: c++

   class A {
     union {
       int i;
       float j;
     };
   } x;

``x.i`` appears as a selection of ``i`` directly out of ``x``.  IL lowering
adds the implied field selection through the unnamed union.

See ``adjust_field_selection_for_anonymous_union_references``.

Automatic Template Instantiation
--------------------------------

When automatic template instantiation is enabled and
``instantiation_flags_in_template_info_file`` is FALSE (an old mode not longer
much used), IL lowering helps the cause by getting certain information about
instantiations passed into the object file.  The instantiation information is
recorded in the flags

* | ``can_be_instantiated``,
* | ``do_not_instantiate``, and
* | ``instance_required``

that appear in variable and routine entries.

IL lowering passes the values of these flags into the object code by generating
tentative definitions of global-scope variables that have names formed by
combining a prefix with the mangled name for the entity for which the flag is
being generated.  The prefixes are

* | ``__CBI__`` for the ``can_be_instantiated`` flag,
* | ``__DNI__`` for the ``do_not_instantiate`` flag, and
* | ``__TIR__`` for the ``instance_required`` flag.

The routine ``make_instantiation_info_var`` is used to create these variables.

For template classes with virtual functions, where the decision on whether or
not to output a definition of the virtual function table is based on the
presence of a definition for the first non-inline virtual function of the
class, a reference to the virtual function table causes a ``__TIR__`` for the
first virtual function to be generated (that requests a definition of the
function, and therefore of the virtual function table, somewhere in the
program).  When a virtual function definition is generated, it references all
the virtual functions of the class, so ``__TIR__`` variables are generated for
them (except for the first virtual function, to avoid the chicken-and-egg
problem of the first virtual function and the virtual function table each
forcing the other to be instantiated, with neither actually referenced from
anywhere).

The prelinker phase notices these names in the object files, and from them can
recover the associated instantiation information.

Guard Code on Variable Template and Template Static Data Member Initializations
-------------------------------------------------------------------------------

When ``TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE`` (in ``targ_def.h``) is
TRUE, "guard" code is placed around initializations of instances of variable
templates and static data members of templates.  Such guard code is necessary
if template instantiation resolution is done by instantiating everything and
then having the (specially-modified) linker discard duplicate copies of
instantiated routines.  Dynamically-initialized template variables and static
data members of templates are a particular problem: because the initialization
code is generated in startup routines, and is undifferentiated from other code
in those routines, a flag is needed to indicate that initialization has already
been done.  After any one instance of the code does initialization, all other
instances will do nothing.

The initialization guard code looks like (cfront-like ABI):

.. code:: c++

   int guard_var;  // Implicitly initialized to 0
   {
     if (guard_var == 0) {
       guard_var = 1;       // Initialization of static data member
     }
   }

For the IA-64 ABI, the code sequence is the one generated by
``add_first_time_test``, which has either calls of ``__cxa_guard_acquire`` et
al.  or is like the above but with the setting of the guard variable after the
initialization.

If the template variable or static data member is a specialization, its
initialization should take precedence over any other (non-specialization)
initializations.  For that case, the guard variable is put out as a definition
with an initial value of ``-1``.  That disables the construction guard code for
all other instances.

Destruction is not a problem, since all destructions of static entities are put
on a list to be done at the end of program execution.  The entry is only put on
the list exactly once when and if the initialization is done.

Runtime Type Information
------------------------

Runtime type information is needed for exception processing and for the RTTI
features of the C++ language.  IL lowering generates special ``typeinfo``
variables for each type that requires such information.

In the cfront-like ABI, note that the runtime ``typeinfo`` is not the same type
as the programmer-visible ``type_info``; the runtime ``typeinfo`` contains a
``type_info`` structure as a member, and has other information besides.  The
``typeinfo`` variables have mangled names of the form

    | ``__T_``\ *mangled-type-encoding*

(except static ``typeinfo`` variables for non-class types, which are unnamed).

The variables are ``typeinfo`` structures (this is still for the cfront-like
ABI):

.. code:: c++

   struct typeinfo {
     type_info       tinfo;      // User-visible type_info structure
     const char      *name;      // Null-terminated string for name
     char            *id;        // Id object pointer
     base_class_spec *bc;        // Array of base class information
   };

Each pointer field is NULL if it is not applicable.  In versions preceding
2.38, there was an additional field after ``id`` which contained a destructor
pointer.  It was eliminated when the\ ``__throw_setup_dtor`` runtime routine
was added.  In versions preceding 2.41, the ``name`` field was not const (it
was made const when const string literals were implemented).

In the IA-64 ABI, the ``type_info`` type visible to the user is the same as the
implementation type, and it has a number of derived classes that provide
additional information for pointer types, class types, etc.  as described in
the ABI spec.  ``typeinfo`` variables have mangled names beginning with
"``_ZTI``".

If the type represented by a ``typeinfo`` variable is a polymorphic class, the
``typeinfo`` variable is external if and only if the virtual function table
variable for the class is external, and the ``typeinfo`` variable is defined
(i.e., initialized) if and only if the virtual function table is defined.  For
other cases (non-polymorphic classes and non-classes), the ``typeinfo``
variable is always static and always defined.

In the IA-64 ABI, the typeinfo variable is made a COMDAT so that there will be
only one copy in the linked program even when the virtual function table
heuristic does not place the definition in a single file.  Likewise, the name
string for a ``typeinfo`` variable is placed in a separate variable with a
predictable mangled name (beginning with "``_ZTS``"), which is made a COMDAT so
that all instances of the name are shared.

In the cfront-like ABI, when the ``typeinfo`` variable is static, and the
type is not an internally-linked class, it is possible that the same type
is represented by several ``typeinfo`` variables.  [#f5]_ In those cases,
the object id pointer is used to indicate that those ``typeinfo`` entries
all correspond to the same type: the object id pointer is non-NULL and
points to an id object variable of type ``char`` whose name has the form

 ``__TID_``\ *mangled-type-encoding*

This variable is put out as a tentative definition and therefore there will be
only one copy of a given id object variable in the executable program.  Two
``typeinfo`` variables are considered to represent the same type if their id
object pointers are non-NULL and equal to one another.

In the cfront-like ABI, for classes that have base classes, the base class
array field points to a (static) variable initialized to an array of entries
with the following structure:

.. code:: c++

   struct base_class_spec {
     typeinfo         *tinfo;  // typeinfo for base class
     short            offset;  // Offset of base class in derived class
     unsigned char flags;      // Flags
   };

Only direct, virtual, and ambiguous base classes of the derived class
appear in the base class list.  The flags byte contains a bit set:
``0x01`` indicates a virtual base class, ``0x02`` is set to mark the
last entry in the array, ``0x04`` indicates a public base class, ``0x08``
indicates an ambiguous base class, and ``0x10`` indicates a direct base
class.

When ``ABI_CHANGES_FOR_RTTI`` is FALSE, an older form of the ``typeinfo`` entry
is generated, which is adequate for exception handling but not for RTTI.  The
differences are as follows:

* | The ``type_info`` and ``name`` fields are not present.
* | ``typeinfo`` variables for nonclass types are put out as tentative
    definitions and rely on default initialization to zero.  Because of the
    name mangling and the fact that the variable is external, there will be
    only one copy of the ``typeinfo`` variable for any given nonclass type in
    the executable program.  No id object variable is needed.
* | The public base class and ambiguous bits in the base class specification
    are not set.

The ``typeid`` operator
^^^^^^^^^^^^^^^^^^^^^^^

The C++ ``typeid`` operator is represented by an expression node with kind
``enk_typeid``.

If the ``typeid`` node has a null expression pointer, the type is known
statically.  IL lowering generates a ``typeinfo`` entry for the static type,
and replaces the ``typeid`` node with a field selection that computes the
address of the ``tinfo`` member of the ``typeinfo`` variable.

If the ``typeid`` node has a non-null expression pointer, the type (a
polymorphic class) must be determined at runtime.

In the cfront-like ABI, IL lowering replaces the ``typeid`` node with a runtime
routine call

.. code:: c++

   __get_typeid((typeid_expr != NULL) ? vptr : NULL)

where ``typeid_expr`` is the ``typeid`` expression (or more precisely the
address indicated by that expression) and ``vptr`` is an expression for the
value of the virtual function table pointer for the class object pointed to.
If ``typeid_expr`` is not reusable, it will be assigned to a temporary and the
temporary will be used in the ``vptr`` expression.  Note that when
``typeid_expr`` is null, a null pointer is passed to ``__get_typeid``; the
latter then throws ``bad_typeid``.

In the IA-64 ABI, the code sequence is entirely inline:

.. code:: c++

   (typeid_expr != NULL) ? (typeinfo*)(vptr[-1]) :
                           (__cxa_bad_typeid(), NULL)

The ``dynamic_cast`` operator
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A ``dynamic_cast`` is represented by an expression node with operator
``eok_dynamic_cast``.  IL lowering rewrites that into

  cfront-like ABI:

    .. code:: c++

     (src != NULL) ? __dynamic_cast    (src, vptr, desired_type, orig_src,
                                         static_type) :
                     NULL

    or

    .. code:: c++

     (src != NULL) ? __dynamic_cast_ref(src, vptr, desired_type, orig_src,
                                        static_type) :
                     NULL

  IA-64 ABI:

    .. code:: c++

     (src != NULL) ? __dynamic_cast    (src, static_type, desired_type, hint) :
                     NULL

    or

    .. code:: c++

     (temp =         __dynamic_cast     (src, static_type, desired_type, hint))
                          ? temp : __cxa_bad_cast()

The second form in each ABI is for a cast to a reference type.  ``src`` is the
expression being cast (or its address, in the reference case); ``vptr`` is the
virtual function pointer for the class (note that it's protected by the
null-pointer check); ``desired_type`` is a pointer to the ``typeinfo`` for the
desired class, or NULL for a dynamic cast to "``void *``"; ``orig_src`` is the
original source pointer in the dynamic cast (the value passed as the first
parameter may have been cast to a base class if the original class has no
virtual function pointer); and ``static_type`` points to the ``typeinfo`` for
the underlying class of the source pointer's static type.

In the IA-64 ABI, a ``dynamic_cast`` to ``void *`` is done as inline code, by
extracting the offset-to-base from the [-2] entry in the virtual function table
and adding that to the object address.

Note that ``dynamic_cast``\ s that do not require runtime processing are
translated by the front end into simple casts or base class casts.

Exceptions
----------

The lowering of exception handling (EH) features can be configured three
different ways:

#. Full lowering: all exception handling features are reduced to C, and
   no support for EH features is required in the back end. This solution
   is easy to use and highly portable, but entails a fairly significant
   overhead in execution cost (15-20%). This mode is required for the
   C-generating back end, and is supported by the minimal runtime
   provided by EDG. It is enabled by the configuration switch
   ``DO_FULL_PORTABLE_EH_LOWERING``.

#. Partial lowering: the data tables of the portable scheme are generated,
   but they use stack offsets for variables (in the portable scheme, all
   addresses are computed at runtime and stored in an object address table).
   ``try``, ``catch``, and ``throw`` are preserved in the IL. Much of the
   minimal runtime provided by EDG would still be applicable, but some
   parts would require changes. This mode is enabled by the configuration
   switch ``GENERATE_EH_TABLES``.

#. No lowering: No data tables are generated; the object lifetime
   information is preserved for use in generating tables in a back end.
   ``try``, ``catch``, and ``throw`` are preserved in the IL. The back
   end makes all decisions about the way in which EH features are
   implemented. This mode is enabled by having both ``GENERATE_EH_TABLES``
   and ``DO_FULL_PORTABLE_EH_LOWERING`` set to FALSE.

It should be noted that in all three schemes the statements and expressions
attached under EH constructs are lowered.  Even in the partial/no lowering
schemes, lowering generates the code needed for things like copying thrown and
caught objects.  Special expression nodes that represent (partially) lowered
exception handling constructs, of kind ``enk_lowered_eh_construct``, are used
to represent the parts of those operations that remain for the back end to
define.

The portable implementation is the one that involves the most transformations
in IL lowering, and the other two schemes are essentially scaled-back versions
of the full-lowering scheme.  Therefore, the description that follows covers
the processing for the full-lowering scheme and indicates differences in the
other schemes.

Exception handling imposes a nontrivial overhead in time and/or space on
programs, including those that do not make use of exceptions.  Accordingly,
exception handling can be enabled or disabled by a command-line option, and
when it is disabled IL lowering will not generate the code or data structures
to support it.

The fully- and partially-lowered implementations do work with the IA-64 ABI,
but they are not compatible with the ABI spec.  For a fully-comforming IA-64
ABI implementation, it is necessary to do the EH implementation in the back
end.

Maintaining Context
^^^^^^^^^^^^^^^^^^^

When an exception is thrown, the stack is unwound to the catch point, and
destructible objects created in the stack frames removed from the stack must be
destroyed by the runtime.  In order to be able to do that, the runtime must
have access to information about the stack frames and the destructible objects
therein.  This information is provided by entries linked into an exception
handling stack.  There are three kinds of stack entries:

* | Entries representing a function.  These are pushed on entry to a function
    and popped on exit from that function, and they point to information about
    the destructible objects in the function.  No stack frame is needed for any
    function that has no destructible objects and no try blocks.
* | Entries for exception specifications of a function.  These are pushed at
    the beginning of each function having an exception-specification (i.e., a
    list of the exception types that the function is permitted to throw), and
    popped at the end of each such function.  The stack frame points to an
    array that describes the types that may be thrown.
* | Entries for ``try`` blocks.  These are pushed at the beginning of each
    ``try`` block and popped on exit from each such block.  The stack frame
    provides space for a ``setjmp`` buffer, and a pointer to an array that
    describes the types caught at each associated ``catch`` handler clause.

The code to push and pop stack frames is generated by

* | ``push_eh_stack_frame`` and
* | ``pop_eh_stack_frame``.

The function and exception-specification stack frames are requested by
``add_eh_function_prologue`` (which also handles epilogue code).

The function stack frame points to several data structures that describe the
destructible objects:

* | The region table, which has entries that describe objects and the cleanup
    to be done for them.  See ``make_region_table_entry``.  The table is an
    array, and an index into that array is called a *cleanup region number*.
    Each entry contains a cleanup region number for a "next" entry in the
    table, which makes it possible to link entries into lists of cleanup
    actions.  At any given point in a function, one can describe the sequence
    of cleanup actions required on termination of that routine by indicating a
    cleanup region number, i.e., the index number of the first entry of a
    linked list of region table entries that enumerates the destructions and
    deallocations to be done, in the order they are to be done.  IL lowering
    generates code to maintain the global variable ``__eh_curr_region``
    appropriately to indicate the set of cleanup actions necessary at each
    point in each function.  See ``assign_to_eh_curr_region``.
* | The array table, which provides additional information about destructible
    objects that are arrays (for example, the array element size).  See
    ``make_array_table_entry``.
* | The object address table, which provides the addresses of destructible
    objects.  The region table identifies objects by indicating their index
    numbers in the object address table.  This extra level of indirection is
    necessary to separate the static parts of the region table, which can be
    used for all activations of a function, from the changeable part, which is
    different for each activation.  (When a function calls itself recursively,
    for example, the exception stack entries for the various activations will
    point to distinct copies of the object address table but to a common static
    copy of the region table.) See ``object_addr_table_index``.

The exception-specification stack frame indicates the types that may be thrown
by pointing to an array whose elements are exception type specifications.  An
exception type specification is a structure defined as follows:

.. code:: c++

   struct exception_type_specification {
     typeinfo      *tinfo;
     unsigned char flags;
     unsigned char *ptr_flags;
   };

The ``tinfo`` pointer points to a ``typeinfo`` variable for a type.  The
``flags`` value is a bit set that indicates modifications of that type, e.g.,
pointer-to the type.  One of the bits is used to indicate the last entry in the
array.  The ``ptr_flags`` field (added in version 2.41) is non-null for
multi-level pointer types.  It points to an array of flags values, each one
describing the cv-qualifiers at one level of a multi-level pointer type.
Again, one of the bits is used to indicate the last entry in the array.

The try-block stack frame contains space for a ``setjmp`` buffer, and a pointer
to an array in which each entry represents a catch handler.  The entries in the
array are exception type specification structures, each one describing the type
to be caught by the associated handler.

Note that local and global static variables are never handled in normal
exception cleanup.  The required destructions for all static entities that have
been constructed are placed on a runtime list, and therefore the cleanup for
those entities need not and should not be described in the region tables.

In the partial-lowering scheme, the region table and array table are generated,
but the object address table is not used.  Instead, the addresses of variables
in the region table and array table are given as stack offsets using
``ck_stack_offset`` constants.  Since there is no object address table, the
code that computes addresses and places them in the object address table is
unnecessary.  No exception stack is maintained; instead, expression nodes with
subkinds ``leck_function_prologue`` and ``leck_function_epilogue`` are inserted
at the appropriate places to indicate to the back end that appropriate
maintenance should be done at those points, and to identify the variables for
the region table and array table.  It is assumed that the back end will also
deal with the exception-specification, if any, for the function.  In place of
assignments to ``__eh_curr_region``, expression nodes with subkind
``leck_cleanup_state`` are used to communicate the new region table entry
number to the back end at each place that changes it.  ``typeinfo`` information
is still put out for all types involved in exception handling operations, but
there will be no references to those entries in the lowered code.  They are
pointed to from the type entries, available for use by a back end.

In the no-lowering scheme, none of the tables are generated (including the
``typeinfo`` entries).  As in the partial-lowering scheme,

* | ``leck_function_prologue``,
* | ``leck_function_epilogue``, and
* | ``leck_cleanup_state``

nodes will be inserted.  The latter have a pointer to a dynamic initialization
entry, rather than a region number, as the indication of the cleanup state to
be established at that point.

When ``INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE`` is TRUE, cleanup state
instructions will be emitted even at the unreachable ends of blocks.  This may
be desirable if the back end is using the cleanup state information to build a
table of cleanup address ranges rather than leaving them as some kind of
executable code.  The instructions generated will have the special kind
``leck_unreachable_cleanup_state``.

Throwing an Exception
^^^^^^^^^^^^^^^^^^^^^

The ``enk_throw`` expression node is rewritten either as

.. code:: c++

   __rethrow();

for the rethrow case (``throw`` with no operand), or as

.. code:: c++

   ((temp = __throw_setup(&typeinfo, size, flags)),
            (initialization, __throw()))

for the normal case.  ``typeinfo`` is the runtime type information variable for
the base type being thrown, ``size`` is the size in bytes of the object being
thrown, and ``flags`` is a bit set that indicates the relationship of the type
thrown to the ``typeinfo`` type -- for example, that the type being thrown is a
pointer to the type indicated by the ``typeinfo`` variable.  The
``__throw_setup`` call allocates space for a copy of the thrown object; the
initialization code makes a copy of the thrown object in that allocated space;
and the ``__throw`` call actually does the throw (and does not return).  If the
thrown object requires destruction, ``__throw_setup_dtor`` is called instead of
``__throw_setup`` (this routine was added in version 2.38).
``__throw_setup_dtor`` has an additional final parameter, which is the
destructor pointer.  If the thrown object is a multi-level pointer,
``__throw_setup_ptr`` is called instead of ``__throw_setup`` (this routine was
added in version 2.41).  ``__throw_setup_ptr`` has a parameter in place of the
flags parameter, which is a pointer to an array of flag bytes that indicate the
cv-qualifiers at each level of the multi-level pointer type.  In cases where
the thrown object is copied to the allocated space using a copy constructor,
and the copy is implicit, ``__exception_started`` is called before the copy
constructor call to tell the runtime to consider the copy constructor call as
"inside" the throw.

In front end versions preceding 2.29, and when ``ABI_CHANGES_FOR_RTTI`` is
FALSE, a runtime routine ``__throw_alloc`` is called instead of
``__throw_setup``.  It has one additional argument: if the ``typeinfo``
type is a class with base classes, the argument is a character string
containing the letter "``Y``" or "``N``" for each base class to indicate
whether the base class is accessible.  Otherwise, it is a NULL pointer.

In the partial-lowering and no-lowering schemes, the ``enk_throw`` node is
preserved.  The dynamic initialization that copies the thrown object is
lowered, using a node with subkind

* | ``leck_thrown_object_address``

to indicate the address in the runtime to which the thrown object should be
copied.  A node with subkind ``leck_exception_started`` is used to mark the
point before a copy constructor call.

See ``lower_throw``.

Catching an Exception
^^^^^^^^^^^^^^^^^^^^^

The routine ``lower_try_block`` lowers the ``stmk_try_block`` statement to
generate code for a ``try`` and its associated ``catch`` clauses.  The code
does roughly the following:

* | Push a try-block entry on the exception stack.  This entry points to an
    array whose elements describe the types to be caught by each ``catch``
    handler.  The index of an entry in the array is the number assigned to
    identify the associated ``catch`` handler.
* | Do a ``setjmp`` in an "``if``" surrounding the dependent statement of the
    ``try``.  This records the address to be jumped back to in the event that
    one of the catch handlers is to be executed.  When the call to ``setjmp``
    returns (the first time), the ``try`` block is executed.
* | For each ``catch`` handler, test the global variable
    ``__catch_clause_number`` (which is set by the runtime before the transfer
    back to this code) to see if it matches the number of the catch clause.  If
    so, copy the value of the caught object (whose address the runtime provides
    in ``__caught_object_address``) to the handler parameter (see
    ``begin_catch_clause``).  Call ``__exception_caught`` to indicate to the
    runtime that the exception is fully caught.  Execute the dependent
    statement of the ``catch``, and, on exit, call ``__free_thrown_object``
    (see ``cleanup_on_exit_from_catch``).
* | At the end of the ``try`` block, whether reached from the ``try`` or from a
    ``catch`` handler, pop the try-block entry off the exception stack.

Note three aspects of using ``setjmp`` to implement ``try``:

#. The size of the ``setjmp`` buffer must be configured to match the target
   environment. See

   * | ``TARG_JMP_BUF_NUM_ELEMENTS``,
   * | ``TARG_J _BUF_ELEMENT_INT_KIND``.
   * | ``TARG_JMP_BUF_ELEMENTS_ARE_FLOAT``, and
   * | ``TARG_JMP_BUF_ELEMENT_FLOAT_KIND``

   in ``targ_def.h``.
#. The ``setjmp`` is represented as a runtime routine call.  This may
   necessitate some sleight of hand from a back end if ``setjmp`` is not really
   a runtime routine.
#. Some optimization may have to be suppressed for the region of the ``try``
   block.  In C, on return from a ``setjmp`` only ``volatile`` variables are
   guaranteed to be set correctly.  The code generated by IL lowering, however,
   assumes that all variables are set correctly, which means that variables
   cannot be kept in registers over calls in any code in which exceptions might
   be thrown.  The ``contains_try_block`` flag in the routine entry is a
   partial solution.  See also the ``modified_within_try_block`` flag.

In the partial-lowering and no-lowering schemes, the ``stmk_try_block``
statement is preserved, with things under it lowered.  The initializations of
``catch`` parameters are lowered, with a node with subkind
``leck_caught_object_address`` used to represent the address in the runtime
from which the caught object should be copied.  (Note that the implicit
indirection inserted for reference parameters in the full-lowering approach is
not inserted in these modes; a back end can check the handler parameter type
and generate such an indirection if one is necessary.) A node of subkind
``leck_exception_caught`` is used to indicate the point (after the
initialization, if any) where the exception is considered fully caught.  On
exit from a ``try`` or ``catch`` (by flowing off the end or by ``goto`` or
``return``), a node of subkind ``leck_try_epilogue`` or ``leck_catch_epilogue``
is inserted.

Unordered Evaluation
^^^^^^^^^^^^^^^^^^^^

The C and C++ language rules do not fully define the order of evaluation of
parts of expressions.  For example, in "``A(1) + A(2)``" the evaluation of
``A(1)`` could occur before or after the evaluation of ``A(2)``.  When
destructible temporaries appear in such contexts, the order of construction and
destruction of the temporaries cannot be predicted from the language rules
alone, e.g., the temporary for ``A(1)`` could be constructed before or after
the temporary for ``A(2)``.  Such temporaries are called "unordered"
temporaries, and the dynamic initialization entries for them are marked as
such.  These temporaries cause a problem for a portable implementation, because
is it necessary when indicating the cleanup to be done at the point of
construction of ``A(1)`` or ``A(2)`` to indicate that the other temporary might
or might not already have been constructed.  This is done by adding a
conditional flag for each temporary, which indicates whether or not the
construction has been done, and having the cleanup position in the cleanup
region table include destructions for both temporaries.  The runtime then tests
the conditional flags to indicate whether the destructions should actually be
done.  This method, of course, involves a time and space overhead, so the flag
``DO_UNORDERED_EH_PROCESSING`` can be set to FALSE to switch it off.  It can be
switched off if the back end will do the constructions in the canonical order
indicated by the IL, or if the ``next_in_destruction_list`` linkage of the
initializations is changed before IL lowering to match the order that will
actually be used by the back end, or if the back end will use some different
technique to accomplish the same result.  Note that when using the C-generating
back end, the order in which temporaries are constructed depends on the C
compiler that will be used, so if one has detailed information about the order
in which it will evaluate expressions, one may be able to switch off this
processing.  The unordered processing is turned off in the no-lowering scheme.

Placement Delete
^^^^^^^^^^^^^^^^

When a placement ``new`` is done, and there exists a corresponding
``operator``\ ``delete`` function (one with the same parameter types as the
``operator``\ ``new``, ignoring the first parameter), if an exception is thrown
before the initialization associated with the ``new`` completes, the
``operator``\ ``delete`` is called to deallocate the storage.  This is
implemented by wrapping the code that performs the initialization inside an
internal ``try`` block.  If an exception is thrown while executing the ``try``
expression, the associated ``catch`` expression, a call of the delete routine,
is executed.  In the fully-lowered scheme, this internal ``try`` block is
expanded into the same code sequence used for a statement-level ``try`` (but as
part of an expression).  In the partial-lowering and no-lowering schemes, the
internal ``try`` is indicated by an expression node with subkind
``leck_internal_try``.  See ``make_internal_try_expr``.

Lowering ``extern inline``
-------------------------------

``When LOWER_EXTERN_INLINE`` is TRUE, ``extern inline`` functions are processed
specially.  In the cfront-like ABI, they are lowered to normal static inline
functions; this mainly involves just a change of the storage class in the
routine entry.  In the IA-64 ABI, they are left external but put into a COMDAT
so that multiple copies are folded into one by the linker.

The code generated in ``LOWER_EXTERN_INLINE`` mode in the cfront-like ABI does
not quite conform to the standard, because it does not ensure that when the
address of an ``extern inline`` function is taken the address is the same
across different compilation units.  Setting ``INSTANTIATE_EXTERN_INLINE`` to
TRUE (instead of setting ``LOWER_EXTERN_INLINE``) provides standard-conforming
behavior: ``extern inline`` functions are treated like template instances, and
the template prelinker assigns the "instantiation" of the function to a single
file, thus ensuring that if its address is taken it is the same everywhere.

Two transformations that apply to all extern inline functions, whether
``LOWER_EXTERN_INLINE`` is specified or not:

* | For functions that have local static variables, the static variables are
    promoted out of the function and made external (and given mangled names),
    so that all copies of the function will refer to a common set of static
    variables.  If the static variables are initialized, the initialization
    guard variables are also made external, with mangled names that begin with
    ``__LSG__`` (cfront-like ABI) or ``_ZGV`` (IA-64 ABI).
* | If ``ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS`` is TRUE, similar processing
    is done for string literals: they are moved into external variables with
    mangled names so that every copy of the extern inline function gets the
    same address for the same string literal.  This is off by default in the
    cfront-like ABI, even though it should be TRUE for full conformance to the
    C++ standard, because it's expensive way out of proportion to its benefit.
    Not being able to count on a COMDAT-like mechanism, the cfront-like ABI
    must put out the variables for the string literals as tentative definitions
    (therefore uninitialized), and initialize them at each point of use.  It's
    not at all pretty, especially in small inline functions, and it's a feature
    that hardly anyone ever counts on.

Inlining
--------

As an option, IL lowering can do inlining of function calls.  That is, when it
encounters a call of a function declared ``inline``, it can replace the call
with IL corresponding to the body of the function with the parameters replaced
by the corresponding arguments.  When a function call occurs as a statement,
the statements of the function body are inserted in place of the call.  When
the function call occurs within an expression, the body of the function is
rewritten as one large expression and that expression is inserted in the proper
place in the containing expression.  It is not always possible to do this sort
of inlining: there are certain constructs (e.g., loops) that cannot be rendered
in expression form.  Even when the inlining is done at the statement level,
there are certain constructs that are not practical to inline.  Calls that
cannot be inlined are left in their original call form, and an out-of-line copy
of the function is used.  A remark is issued.

Inlining of this style has some serious limitations.  Aside from the
impossibility of inlining certain function bodies, there is also the problem
that inlining at this level is done without knowledge of the target
architecture, and therefore without the ability to know whether the inlined
version of a call is preferable to the original version.  The inlined version
might be ridiculously large, or might be worse in some other way.  Inlined
calls produce extremely convoluted expressions, which stress back ends and C
compilers.

In short, if you want inlining (and you do, for C++), you probably want to do
it some other way.  The version EDG supplies is provided as a convenience for
those who use the C-generating back end and whose C compilers do not handle
inlining.  It's intended to give roughly the same functionality as cfront, and
EDG does not plan to enhance it in any substantial way.

The code to perform inlining is contained in ``inline.c``, with the associated
declarations in ``inline.h``.  Inlining of function calls is enabled by the
configuration switch ``MINIMAL_INLINING``.  The default is to inline only when
using the C-generating back end.

Inlining is done on code that has already been lowered, which is to say on C
code.  When the end of the lowering of a function's body is reached,
``set_up_routine_for_inlining`` is called to determine whether the routine can
be inlined.  If so, the ``inlinable`` flag in the routine entry is set.  Note
that this means that a routine call cannot be inlined until the full definition
of the function has been seen and lowered.  Also note that lowering is not done
if any errors are detected, so inlining is always done using correct code.  A
function is disqualified for inlining immediately if any of the following are
true:

* | The function has local static variables.
* | The function has local constants.
* | The function has local types.
* | The function has block scopes.
* | The function includes pragmas.
* | The function has an ellipsis in its parameter list.

A function that passes these tests is eligible to be inlined.  When a call is
seen, ``do_inlining_of_call`` is invoked to do the expansion.  The expansion is
done in three steps:

#. ``set_up_variable_remapping_for_inlining`` makes temporary variables
   for the parameters and local variables of the function, and generates
   assignment code to initialize each parameter temporary with the
   corresponding argument expression. If the argument is constant-valued
   and the parameter is not modified in the function, no temporary is
   needed; the constant will be used directly.

#. ``expand_statement_inline`` does the expansion of the function body.
   It makes a copy of the statements and expressions of the function,
   replacing references to the parameters and local variables with the
   appropriate temporaries or constants. If the function call being
   inlined appears at the statement level, the copy is made in statement
   form. Otherwise, the function body is translated into expression
   form as the copy is made. Expression statements are copied as
   expressions; a sequence of statements is formed via comma operations;
   and ``if`` statements are rendered as "``?``" operations. Loops cannot
   be translated to expression form, and therefore can be inlined only in
   the statement mode. ``goto``\ s and labels generally preclude inlining
   in both modes, but the special case of a ``goto`` to a label
   immediately following it is allowed and eliminated. ``return``\ s
   are allowed only at the end of the function at the top level (or as
   the last statement in a block that is the last statement in a block
   ... that is the last statement of the top-level block of the function).
   Dynamic initialization of local variables is translated to
   assignments, but aggregate initialization precludes inlining.

#. ``finish_variable_remapping_for_inlining`` adds the created temporaries
   to the calling context scope, and then the statement sequence or
   expression tree is inserted into the IL tree, replacing the original
   call.

Since the inlining operation is an attempt to inline the function, one that may
fail, the statement sequence or expression tree for the inlined call is built
off to the side, and attached to the IL tree only once the inlining operation
is completed successfully.  Likewise, the temporaries created for parameters
and local variables are not entered into the scope of the call until the end of
the operation.  If an inlining operation fails, the original call is left in
place and the routine is marked as requiring an out-of-line copy.  (An
out-of-line copy is also required if the address of the routine was taken.) If
inlining of a call fails because of something that precludes *all* inlining of
the function, the ``inlinable`` flag of the routine is cleared so no further
attempts will be made.  If the inlining operation failed only because of the
context (e.g., the function includes a loop and the call is inside an
expression), the ``inlinable`` flag is left TRUE.

As the function body is copied with substitution for the parameters and local
variables, new constant expressions may be created.  That is, when a constant
argument is passed for an unmodified parameter, the references to the parameter
in the function body are replaced by the proper constant value.  That may
produce operations whose operands are constant (e.g., ``p + 1`` becomes ``1 +
1``, which can be simplified to ``2``).

This substitution and folding is done by
``adjust_copied_expression_for_inlining``, which is called from
``copy_expr_tree`` as each node in the tree is copied.  The list of remappings
to be done is built by ``set_up_variable_remapping_for_inlining`` and pointed
to by the static variable ``variable_remappings_for_inlining``.
``get_var_remapping_for_inlining`` consults the list to find the applicable
remapping for a variable.

More dramatic simplification is possible for short-circuiting operators like
"``?``" and for ``if`` statements (whether they are rewritten in expression
form or not).  If the tested expression is constant, parts of the expression
tree or statement tree are discarded accordingly.  For example, when parameter
``p`` is replaced by ``1``,

.. code:: c++

   p ? i : (j + 1)

becomes simply ``i``, and

.. code:: c++

   if (p > 0) i = 1; else i = 2;

becomes simply ``i = 1``.  This is handled by
``copy_and_simplify_short_circuited_operation`` for the operator cases, and in
``expand_statement_inline`` for the ``if`` statement.

Arguments can be considered to be constant even if they are not literally
constants.  For example, the address of a local variable is not a compile-time
constant, but it is constant over the duration of a call, and therefore can be
considered constant-valued for purposes of parameter remapping.

The ``this`` parameter of constructors is given special treatment so that the
allocation sequence at the beginning of constructors will be eliminated when
the constructor is operating on a known variable.  According to the normal
parameter rules, ``this`` in a constructor is modified within the function and
therefore a temporary must be used.  However, we know how constructors work,
and if there is no assignment to ``this`` in the constructor we know the only
modification is one that will be eliminated when the constructor is called with
a non-null argument for ``this``.  In such cases, the ``this`` parameter is
remapped directly to a constant, and the allocation sequence is eliminated by
the normal short-circuit optimization.

Lowering ``thread_local``
-------------------------

The ``thread_local`` keyword was added in C++11 and the dynamic initialization
and destruction of objects with ``thread_local`` storage requires lowering
support.

If the back end has the capability to execute code at thread creation time,
then it is most efficient to set the configuration macro
``USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES`` to FALSE in which
case lowering creates one or more initialization routines (depending on the
setting of ``SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS``) for each
dynamically-initialized variable with ``thread_local`` storage in the
translation unit and sets ``il_header.thread_local_dynamic_init_routines`` to
point to the list (in the proper order).  It is the responsibility of the back
end to ensure that this list of routines is invoked at thread creation time.

In most cases, however, the compiler won't have the capability to execute code
at thread creation time, in which case
``USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES`` should be set to
TRUE.  In such configurations, each use of an object with ``thread_local``
storage duration that may be dynamically initialized is re-written to invoke
the "wrapper" routine for the object (see ``lower_thread_local_variable``).
Lowering creates a "wrapper" routine (see
``thread_local_wrapper_for_variable``) that invokes an "initialization" routine
(if one exists) and then returns a pointer to the ``thread_local`` object.
This ensures that each ``thread_local`` object is properly initialized (though
at considerable run-time cost).  Mangled names for wrapper routines contain the
mangled name of the ``thread_local`` object along with a prefix ( ``_ZTW`` in
the IA-64 ABI and ``__TWR__`` in the Cfront ABI).

Note that although "wrapper" routines are only required for dynamically
initialized ``thread_local`` variables, the lack of "wrapper" routines for
``thread_local`` variables with external linkage that are not dynamically
initialized results in a non-compliant implementation (i.e., the standard
requires that all ``thread_local`` variables be initialized before the first
odr-use of any ``thread_local`` variable, but the use of a statically
initialized ``thread_local`` circumvented the initialization process).  As a
result, the ``all_thread_locals_have_wrappers`` global variable was introduced
in 4.10, and when set to TRUE, results in standard compliant behavior.  The
previous behavior matches g++'s behavior and is achieved by setting
``all_thread_locals_have_wrappers`` to FALSE.

The "initialization" routines that wrappers call perform the actual
initialization for the ``thread_local`` objects (and incorporate a
``thread_local`` guard variable to ensure the initialization is performed only
once per thread).  When the back end supports weak references,
``LAZY_INITIALIZATION_USES_WEAK_REFERENCES`` should be set to TRUE, in
which case each "initialization" routine is aliased to the ``__tls_init``
routine that initializes that particular ``thread_local`` object.  In cases
where a ``thread_local`` object is used in one translation unit and defined in
another, a reference to an initialization routine is created in the first
translation unit but there may or may not be a definition for the
initialization routine in the second routine (it will be aliased to the
``__tls_init`` routine in that translation unit only if the ``thread_local``
object is dynamically initialized).  When weak references are used wrapper
routines need to allow for the case where an initialization routine is
``NULL``.  For back ends that do not support weak references, a do-nothing
initialization routine is created for this case (see
``make_null_thread_local_init_routine_for_variable``).  Mangled names for
initialization routines contain the mangled name of the ``thread_local`` object
along with a prefix (``_ZTH`` in the IA-64 ABI and ``__THI__`` in the Cfront
ABI).

The ``__tls_init`` routines themselves (or ``__tls_init_N`` in
one-instantiation-per-object mode) are created during the lowering of
file-scope dynamic initializations (see :ref:`dynamic-init`).

In a manner similar to the handling of destructions of static entities,
destructions of file-scope objects with ``thread_local`` duration are
registered with the run-time library through calls to ``__cxa_thread_atexit``
(for the IA-64 ABI) or ``__record_needed_thread_destruction`` (for the Cfront
ABI).  The run-time library is then responsible for invoking these destructors
when the thread terminates (see ``record_needed_destruction``).

Lowering ``ifunc``
------------------

The GNU ``ifunc`` attribute maps to the ``STT_GNU_IFUNC`` symbol type in the
ELF standard and is not available on many architectures.  The ``STT_GNU_IFUNC``
symbol type allows a routine's symbol to be determined dynamically at load
time.  At load time, the routine specified as the resolver (in the ``ifunc``
attribute) is invoked (once) and returns a pointer to the routine that will be
used to resolve all instances of that symbol for that execution of the
executable.  This provides a very low overhead mechanism to select one of a
family of functions (typically based on the underlying CPU architecture).
Here's an example (with ``--gcc --gnu_version 40600``):

.. code:: c++

     int printf(const char *,...);
     void target() {
       printf("Here\n");
     }
     static void (*resolver(void))(void) {
       return (void(*)(void))&target;
     }
     void source() __attribute__ ((ifunc ("resolver")));
     int main() {
       source();   // Prints "Here"
     }

For back ends that don't support the ``STT_GNU_IFUNC`` symbol, an approximation
of the run-time behavior can be achieved by setting ``LOWER_IFUNC`` to
TRUE.  In that case, lowering will convert the routine with the ``ifunc``
attribute into a "wrapper" routine as such:

.. code:: c++

     decltype(source) resolver_result = source;
     source(args...) {
       if (resolver_result == source) {
         resolver_result = (decltype(source))resolver();
       }
       return *resolver_result(args...);
     }

This has the effect of invoking the resolver only once (though at run time
rather than at load time).  Invocations of ``source`` incur an extra function
call and indirection, but lowering re-writes references to ``source`` so they
are dispatched through the resolver variable whenever possible (in which case
the only overhead is an extra indirection).

Note that there are a few caveats:

* | The re-writing of calls to avoid the wrapper routine only occurs once the
    declaration with the ``ifunc`` attribute has been seen; any reference prior
    to that point will be unlowered (and will hence invoke the wrapper).  That
    can be an issue if, for example, the declaration with the ``ifunc``
    attribute is only in a library's implementation (and not in the shared
    header file).  In that case, code that is compiled with the shared header
    file will contain references to the wrapper routine.
* | Constants (i.e., ``ck_address``/``abk_routine``) that refer to the
    ``ifunc`` are not re-written in lowering, so they'll always point to the
    wrapper routine.

See ``lower_ifunc_routine`` and ``lower_ifunc_expr`` for more information.

Lowering GNU function multiversioning
-------------------------------------

GNU supports function multiversioning in version 4.8.0 and later via the
``target`` attribute in C++ mode.  Detailed documentation is available here:
``http://gcc.gnu.org/wiki/FunctionMultiVersioning``.  The ``target`` attribute
allows multiple versions of a function definition to be supplied, and the
choice of which version to call is determined at load time based on
characteristics of the machine where the program is running.  The choice is
made by a compiler-generated (when lowering is enabled) "resolver" routine that
selects the best routine from the target-specific versions.  The resolver
routine is executed once at load time (because it is associated with an
``ifunc`` routine) to determine which target-specific routine to use for the
particular CPU.  For back ends that don't support ``ifunc``, see
``LOWER_IFUNC``.

For example (assuming ``USE_X86_FUNCTION_MULTIVERSIONING`` is ``TRUE,`` with\
``--gnu_version 40800``):

.. code:: c++

     __attribute__ ((target("default")))     int foo () { return 222; }
     __attribute__ ((target("arch=corei7"))) int foo () { return 777; }
     __attribute__ ((target("avx")))         int foo () { return 333; }
     int main () {
       int (*fp)() = foo;
       return fp() != foo();
     }

Would generate lowered pseudo-code like this:

.. code:: c++

     int foo_default () { return 222; }  // "default" foo
     int foo_corei7 () { return 777; }   // "corei7" foo
     int foo_avx () { return 333; }      // "avx" foo
     // Resolver function returns address of specific function
     static (int *foo.resolver()) { // lowering-generated resolver function
       resolved_foo = // one of foo_default, foo_corei7, foo_avx
       return resolved_foo;
     }
     int foo.ifunc() __attribute__((ifunc("foo.resolver"));
     int main () {
       int (*fp)() = foo.ifunc;    // &foo lowered to &foo.ifunc
                                   // Dynamic loader invokes foo.resolver
                                   // to select which specific foo.
                                   // foo.ifunc is invoked once during
                                   // startup.
       return fp() != foo.ifunc(); // call of foo is lowered to foo.ifunc
     }

Note that at present, lowering is only performed when
USE_X86_FUNCTION_MULTIVERSIONING is TRUE (the rules that are used to determine
at run-time which of a set of target-specific functions are machine-specific
and only the x86 case is implemented).

See ``lower_mv_routine``, ``lowered_mv_routine``, and ``create_mv_resolver``.

How is IL Lowering Done?
========================

Sequencing
----------

All the individual transformations done by IL lowering are relatively
straightforward.  What makes the job of IL lowering so difficult is the fact
that it must operate under some very difficult sequencing constraints.

They come about because IL lowering must be done on a per-function basis, so
that individual memory regions can be lowered and then written to a file.  That
means that when function scope memory regions are lowered, the file scope
entities that they reference are not yet lowered.  And they cannot be: since
the file-scope entities are still active entities in the compilation, if they
were modified by IL lowering it would disturb the rest of the compilation.

To pick an example: to lower a virtual function call in a function scope, one
needs to be able to reference the virtual function pointer field in the class
entity.  That field is added by IL lowering.  But the class type is in the file
scope memory region, and therefore has not yet been lowered at the time the
virtual function call is lowered.

Another example: IL lowering changes reference types into pointer types.
However, mangled names of functions must be based on the original types,
including reference types.  Therefore, mangled names must be generated before
reference types are changed to pointer types.

Another example: the variable that contains a virtual function table must be
available when constructors and destructors for classes are lowered.  However,
it is not possible to know at that point in the compilation whether or not the
definition of the virtual function table should be put out.  In fact, it's not
possible to tell whether it should be internal or external.  Therefore, virtual
function table variable definitions cannot be done at the time the variable
declarations are done.

In short, lots of processing that would be fairly straightforward if the entire
IL tree could be lowered at once becomes convoluted because some parts of the
tree have already been lowered, some haven't, some cannot be yet, etc.

One technique that helps is the concept of "prelowering" classes.  This is
processing that's done as soon as possible on completed class definitions.  It
does not modify the class, but it adds information that is needed to use the
class in lowering code:

* | The type-as-subobject (a copy of the type with any virtual base classes
    removed).
* | The fields that make the base classes, virtual base class pointers, and
    virtual function table pointer of the class explicit.  (Note that this
    change affects the field list of the type, which would seem to be a
    modification; however, since the fields are not in the symbol table, they
    cannot be found by the front end.  It does mean that front end code should
    not traverse the field list to visit all the fields.  It should use the
    symbol list instead.)
* | Variables for virtual function tables.  These are all created as ``extern``
    declarations, and they are changed to definitions or to static variables
    later if that is appropriate.

See ``prelower_class_type``.

Another technique is to not clear C++-specific fields when they are
lowered, where that's possible.  One example: the implicit "``this``"
parameter type in a routine type entry becomes an explicit first parameter
type when lowered.  However, the pointer to the implicit type is not
cleared.  That allows code that wants to see whether or not a function is a
nonstatic member function to fetch the "``this``" parameter type without having
to know whether or not the type has already been lowered.  One consequence
of this technique is that back ends cannot count on having the C++-specific
fields cleared, and should therefore not look at them.

The IL Lowering Flag
--------------------

The IL lowering flag is a two-state flag that appears in the prefix that
precedes each IL entry.  It is flipped from 0 to 1 when the associated entry is
visited during IL lowering.  This allows IL lowering to know that it has
already processed a given entry and thereby avoid looping when there are cycles
in the IL graph.

This flag is similar to the ``il_walk_flag`` used by the IL walk routines, but
is kept separate so that IL lowering is not forced to visit all of the IL tree
even if it knows that certain parts cannot contain entries that will be
modified by lowering.  It should be noted that even though every entry has a
flag, not all entries' flags are maintained by IL lowering.  For example, IL
lowering does not maintain the flag on expression nodes, because it knows that
it will visit each expression node exactly once.

Why not use the standard IL walk routines to do the traversal? The problem is
that the IL walk routines cannot co-exist with processing that changes the
structure of the IL tree.  Since IL lowering can eliminate entries, move them
around, add entries, and swap entries, the IL walk routines could get into
loops, get lost, or process entries more than once.  Therefore they are not
suitable, and IL lowering has its own IL traversal code.

Orphaned Entries
----------------

Type entries and static variables that are local to a function require some
special handling.  All type entries and all static variables are allocated in
the file scope memory region.  However, types and static variables that are
local to a function are placed on the types and static variables list of a
function or block scope.  Those are unusual lists: the function scope entries
(in a function scope memory region) point to lists that are entirely in the
file scope memory region.  Yet, the entries pointed to are not really part of
the file scope.  They might, in fact, not be referenced from anywhere except
the function scope memory region.  Such entries are "orphaned" in the file
scope: their parents are processed, written out, and removed from memory, but
the children remain in the file scope, unattached to the rest of the file scope
IL tree.

There is special processing in the IL write, read, and walk routines to deal
with such orphans, but special handling is also required in IL lowering.

Such entries clearly must be lowered at some point.  Do we lower them when the
function scope is lowered, or do we lower them when the file scope is lowered?
Well, lowering them with the function scope is impractical, since it's hard to
tell which of the file-scope references are orphans and which are not.  Even if
we know that a static variable, say, is an orphan, how much of its subtree do
we process? Is its type an orphan, or just a normal file-scope type?

So it's easier not to do lowering on orphans.  The processing then lowers
entries in the function scope, and leaves all those in the file scope alone.
Whenever a pointer hops into the file scope memory region, IL lowering leaves
that entry alone, regardless of whether or not it might be an orphan.

This solves one problem but creates another.  If there truly are orphaned
entries, how will the IL lowering for the file scope find them to lower them?
The answer is that orphans or potential orphans are recorded on a special list,
using support routines and data structures provided by the IL support
components.  When a pointer hops into the file scope memory region, the entry
pointed to is recorded as a potential orphan, and at the end of the lowering
for the file scope all the orphans are visited.  If they turn out not to be
orphans, the IL lowering flag will indicate that they have already been visited
and they will not be processed again.

See ``lower_orphaned_entries``.

Maintained Context
------------------

While IL lowering works through a top-level scope, it maintains a stack that
shows the nesting of scopes for the present position (the spot for which
lowering is being done).  This makes it easy to find the innermost enclosing
scope or the innermost enclosing function at any given moment, for example to
create a temporary in the innermost scope.  The global variable
``curr_context`` points to the top of the context stack.

Object Lifetimes
----------------

The object lifetime entries in the IL provide information about regions of the
program on exit from which automatic destructions of class objects are done.
The destructions to be done appear in a list attached to the object lifetime
entry (in the form of dynamic initialization entries).  IL lowering uses this
information to insert destructor calls on exit from those regions (whether by
flowing off the end of the region or branching out of it via a ``goto`` or the
like), and to build region table information for exception handling.

As IL lowering works through the IL, it keeps track of the current position in
cleanup terms, which is to say the latest initialization encountered that
requires automatic destruction.  This is maintained in the
``latest_initialization`` field of the current context.  Given the pointer to
the latest initialization, ``gen_cleanup_actions`` and its subroutines can walk
up the cleanup chain, generating the appropriate destructions.  In the case of
a ``return``, the walk up the chain continues until the lifetime for the
function scope is reached.  In the case of a ``goto``, the cleanup continues
until the lifetime indicated in the ``goto`` statement is reached (that
lifetime is determined by the front end, and is the innermost lifetime that the
``goto`` and the label have in common).

At the beginning of lowering of a block, the object lifetime for the block and
for any expression-temporary lifetimes immediately within it are processed.
This processing (which is performed by ``begin_object_lifetime``) involves
allocating an entry of type ``a_destructible_entity_descr`` and attaching it to
each dynamic initialization on the lifetime destructions lists.  These entries
are used to record information that will be needed when generating the
initialization and the later destruction.  In particular, the position of the
entity (in initialization position form; see below) is determined and recorded
in the entry at the time of initialization.  That position is then used when
generating the destruction code.  The beginning-of-lifetime processing also
includes deciding whether the entity will require a conditional flag to
indicate that the construction has been done.  If one is required, the variable
for it is created and code to initialize it to zero is inserted.

At the beginning of block-after-label lifetimes, similar processing is done,
but it takes a bit of effort to get it done at the right time, because there is
no explicit indication of a new lifetime on a statement that ushers in a
block-after-label lifetime.  One must note, at the beginning of the previous
block or block-after-label lifetime, the next block-after-label lifetime in
sequence, and watch for the appearance of the associated statement during the
normal processing of statements.  Also at labels (including switch case
labels), long lifetime temporaries must be destroyed (and when exceptions are
enabled, the region table is adjusted accordingly).  (For this reason,
block-after-label lifetimes can begin at ``stmk_switch_case`` statements in
long-lifetime-temporaries mode.)

Initialization Positions
------------------------

``an_init_pos_descr`` is used to contain a description of a position within an
initialization.  As an aggregate initialization is processed, such a
description is maintained for the current position.  The description can then
be turned into an expression that addresses the indicated position, which is
necessary when generating code to initialize pieces of aggregates:

.. code:: c++

   int j;
   void f() {
     struct {int i; int j;} k[2] = {{1, 1}, {j+1, 2}};
   }

becomes

.. code:: c++

   int j;
   void f() {
     struct {int i; int j;} k[2] = {{1, 1}, {0, 2}};
     k[1].i = j+1;
   }

Insert Locations
----------------

Many transformations work by inserting new pieces of code into expressions or
statement lists.  The insert location for such insertions is described by a
structure of type ``an_insert_location``.  It can describe an insert location
either within an expression or within a sequence of statements.  The code
inserted is typically in units of "statements", even when the insertion is
within expressions.  That is, a lowering routine would assemble something like
``temp = 1`` and that construct would be inserted, either as a free-standing
expression statement, or as a clause in a comma operation added within an
expression.  For the expression insertion case, for example,

.. code:: c++

   i ? (void)A(i) : (void)0

might be modified by insertion of an assignment:

.. code:: c++

   i ? (temp = 1, (void)A(i)) : (void)0

Since the insertion is done in statement units, the callers of the insert
routines need not in general be concerned about whether or not the insert
location is in an expression or a statement.  The low-level routines do the
necessary adjustment, including rendering conditional tests as ``if``
statements or "``?``" operators.

See

* | ``insert_expr``
* | ``insert_statement``
* | ``insert_if_statement``

Reusable Copies
---------------

There are a few cases where an expression that is used once in the original IL
tree is used more than once in the lowered IL tree.  Without special handling,
such cases might cause problems if the expression involved has side effects.
The routine ``make_reusable_copy`` solves the problem: given an expression
tree, it returns a distinct expression tree that produces the same value.  If
the original tree has no side effects, it is simply copied.  If it has side
effects, the original expression is modified to assign the expression value to
a temporary, and the new copy is simply a reference to the temporary.

Note that the new copy does not replace the original expression.  The original
expression continues to be used in its original location, and the copy is used
for the second reference.  Further copies can be made from either of the
expressions.

``make_lvalue_reusable_copy`` does a similar operation for expressions that are
lvalues.  It handles bit-field lvalues specially.

.. _lowering-c-dialects:

Lowering C Dialects
===================

In some newer C modes like C99 or GNU C mode, some features create IL
constructs that are not used in IL for C89.  In order to make things easier for
back ends, the front end includes a C lowering phase, which can rewrite many
(but not all) of these newer C constructs.  Many (but not all) of the
additional IL features are enabled by ``C99_IL_EXTENSIONS_SUPPORTED``.  Most of
the code for this lowering is in ``lower_c99.c``, with related declarations in
``lower_c99.h``.

* | For compound literals, the generated ``enk_temp_init`` node is rewritten as
    an initialized generated variable.  See ``lower_c99_temp_init``.
* | Designated initializers are processed to produce a C89-style aggregate
    initializer.  This is done outside of the primary C99 lowering phase, and
    is enabled by ``LOWER_DESIGNATED_INITIALIZERS``.  ``ck_designator``
    constants remain in the IL after this lowering for initializations of
    members other than the first in unions, which are not expressible in C89
    IL.  See ``lower_designated_initializers``.  Note that there are some
    advantages to not doing this lowering and instead handling the designators
    in a back end, notably that that can avoid some very large IL structures
    for big sparsely-initialized arrays.
* | The ``_Bool`` type is unchanged, but a C89 view of the IL does not consider
    the ``bool_type`` flag and therefore sees the underlying integer.  Compound
    assignment operators whose left-hand operand has type ``_Bool`` are
    rewritten to ensure that the result value is restricted to 0/1, via the
    same routines that do this processing in C++ lowering; see
    ``rewrite_compound_assignment``.  Boolean casts are also handled as in C++;
    see :ref:`il-bool-type`.
* | Complex types are rewritten as structs, imaginary types are rewritten as
    floating-point types, complex constants are rewritten as aggregate
    initializers, imaginary constants are rewritten as floating-point
    constants, and operators and conversions on complex and imaginary types are
    rewritten as runtime routine calls.  This is controlled by
    ``LOWER_COMPLEX``.  See ``lower_c99_nonreal_float_types``,
    ``lower_c99_complex_cast``, and ``lower_c99_operator``.  Also see the file
    ``c99_complex.c`` in the runtime library for descriptions and sample
    implementations of the complex runtime routines.  Note that a back end
    could treat the runtime routines as intrinsics and generate machine
    instructions directly for the calls.  (Again, the C99-centric names are
    historical: These routines also handle complex types appearing in GNU C
    source.)
  |
  | For compound assignment operators, the operation is rewritten in terms
    of the underlying operation, and the result is then converted and
    stored in the destination variable (e.g., ``x += y`` is rewritten as
    ``x = x + y``).  If the left side is an expression with side effects,
    its address is evaluated and stored in a temporary, and the two
    references to the left hand side (to fetch its value as as input to the
    operation, and to store the result) are done by way of indirection
    through the temporary.  See ``rewrite_compound_assignment``.

* | If ``LOWER_FIXED_POINT`` is TRUE, fixed-point operations, types, and
    constants are rewritten into standard C.  Fixed-point types are lowered to
    typedefs for appropriately-size integral types (see
    ``lower_c99_fixed_point_types``).  Fixed-point constants are lowered to
    integral constants (see ``lower_c99_fixed_point_constant``).  Fixed-point
    operations and conversions are lowered to calls to runtime routines (see
    ``lower_c99_fixed_point_operation``, ``lower_c99_fixed_point_cast``,
    ``lower_c99_fixed_point_incr_decr``, and ``rewrite_compound_assignment``).
    The runtime routines are not provided by EDG; they are available from
    Dinkumware, Ltd.  There is a long comment beginning with "Fixed-point
    lowering:" in ``lower_c99.c`` that gives more details on the runtime
    interface.
* | When ``LOWER_VARIABLE_LENGTH_ARRAYS`` is TRUE variable-length arrays (VLAs)
    are rewritten as pointers to storage allocated by the run-time library.
    Storage for a VLA is allocated by a call to ``__vla_alloc``, which is
    produced by the lowering of certain ``stmk_vla_decl`` statements.  The
    storage is freed by calls to ``__vla_dealloc``, which are the result of
    lowering ``enk_vla_dealloc`` expression nodes (which may be produced
    directly by the front end in C modes, or as the result of IL lowering in
    C++ modes).
  |
  | A VLA type can be defined recursively to be either an array type with a
    nonconstant bound, or a normal (constant-bound) array whose element is
    a VLA type.  The lowering process eventually replaces all VLA types and
    pointer to VLA types by pointer types pointing to the underlying
    (non-array) element type of the VLA type.  For example, the type ``int
    [3][n][4]`` is replaced by ``int*`` and the type
    ``double*(*)[3][n][4]`` is replaced by ``double**``.  To avoid certain
    subtle ordering problems, this type replacement is done in two stages.
    First, all VLA types are collected on a linked list (see
    ``record_vla_component_types_for_lowering``) during the main C99 IL
    lowering traversal (this is necessary because these types may only be
    pointed to from an expression node, unlike, e.g., class types which are
    listed on a scope's type list).  After the main tree traversal, the
    actual replacement is done by a call to the function
    ``lower_vla_types``.
  |
  | The length of VLA types must in general be computed at run time.  This
    is achieved by evaluating the array bound expression and storing the
    result in temporary helper variables.  To further simplify the VLA size
    computations in expressions, these variables are updated to hold a
    total count of the number of (non-array) elements for a particular VLA
    type component.  For example, the type ``char[n][m]`` would produce two
    variables, one holding the value ``m`` and the other holding the value
    ``n*m``.  Once created, the IL entries for these variables can be
    accessed through the ``a_vla_dimension`` entries of the associated VLA
    type components.  For VLA types appearing in explicit declarations
    (variable declarations and typedef declarations), the front end adds
    adds explicit ``stmk_set_vla_size`` statement indicating when the value
    of these variables ought to be computed.  However, in expression
    contexts that introduce new VLA types (``sizeof`` and cast expressions,
    as well as compound literals) there are no such statements.  Either
    way, the bulk of the work to produce and compute these variables is
    done in ``lower_vla_dimensions``, which creates all the necessary
    variables for an arbitrarily complex VLA type and returns an expression
    that sets all the variables to their required values.
  |
  | With those variables available, the lowering of various array-related
    operations (subscripting, pointer arithmetic, and run-time ``sizeof``
    expressions) is relatively straightforward.  There is a long comment
    near the  beginning of ``lower_c99.c`` (conditioned on
    ``LOWER_VARIABLE_LENGTH_ARRAYS``) that gives more details on the
     lowering of VLAs.
* | Extended identifiers containing universal character names (``\u``\ *xxxx*),
    whose name strings contain the ``\u`` etc., are rewritten as standard
    identifiers if ``REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING`` is TRUE.  The
    "``\``" character is changed to "``_``".  This is the same processing that
    is done in C++ lowering.
* | Non-constant expressions in aggregate initializers, i.e., those indicated
    by ``ck_dynamic_init`` constants, are rewritten as executable code.  This
    is done via the same routines that do this processing in C++ lowering.  See
    ``lower_c99_stmk_init``.
* | Inline function calls are replaced by their expansions if
    ``MINIMAL_INLINING`` is TRUE.  This is done via the same routines that do
    this processing in C++ lowering.
* | Boolean controlling expressions are optionally normalized to always yield
    an integer value of 0 or 1.  This transformation occurs even in C89 mode
    (and the same processing is done in C++ mode).

Some features are not lowered but use IL constructs that exist in IL produced
by lowering C++, which a back end is likely already to accept:

* | For mixed statements and declarations, the statements and declarations
    appear intermixed in the statement list of a compound statement.
* | For ``long``\ ``long``, there are two new integer kinds (``ik_long_long``
    and ``ik_unsigned_long_long``) and new constants of these kinds.

One feature is not lowered at all:

* | For flexible array members, the last member of a struct type may have an
    array type with an unknown bound.  This probably does not require special
    processing in a back end.



.. [#f1] The configuration flag ``MINIMAL_INLINING`` can be used to enable a
         minimal version of inlining for those who have no better alternative.
.. [#f2] This routine is not present in the cfront runtime.  It would have to
         be added to a cfront runtime library to make the library usable with
         the EDG front end.
.. [#f3] One nit: a function is considered inline only if it is inline in the
         class definition, and not if inline was added on the out-of-class
         definition.
.. [#f4] This feature is not implemented in cfront 2.1, and therefore this
         runtime routine is not present in the cfront 2.1 runtime library.  It
         would have to be added to such a library to make the library usable
         with the EDG front end.
.. [#f5] In the IA-64 ABI, IL lowering avoids generating duplicate
         ``typeinfo`` variables by checking a new needed type against all
         previously generated ``typeinfo`` variables.
