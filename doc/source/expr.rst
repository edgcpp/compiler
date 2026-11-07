===================
Expression Scanning
===================




``expr.c`` and ``expr.h`` contain the code and associated declarations to scan
expressions.  ``exprutil.c`` and ``exprutil.h`` contain utilities related to
expression scanning, for processing other than the actual syntax analysis.
``overload.c`` and ``overload.h`` contain code to do overload resolution and
conversions.

The expression routines function as an expression-scanning utility for the rest
of the front end.  They provide a set of simple interface routines that are
called to scan various kinds of expressions:

* | ``scan_integer_expression`` scans an integral expression, e.g., the
    selector expression for a ``switch`` statement.
* | ``scan_void_expression`` scans a "void" expression, one whose value is
    thrown away, e.g., an expression statement or the incrementing expression
    in a ``for`` statement.
* | ``scan_default_arg_expr`` scans a default argument expression.
* | ``scan_template_argument_constant_expression`` scans a nontype argument of
    a template reference.
* | ``scan_return_expression`` scans the expression in a ``return`` statement.
* | ``scan_case_label_constant`` scans a constant expression for a switch case.
* | ``scan_constant_dimension_expression`` scans a constant array bound.
* | ``scan_pp_expression`` scans a preprocessing expression, e.g., an
    expression in an ``#if``.
* | ``scan_integral_constant_expression`` scans an integral constant
    expression.
* | ``scan_constant_initializer_expression`` scans an initializer that is
    required to be a constant.
* | ``scan_member_constant_initializer_expression`` scans an initializer for a
    member constant (integral or enum) in C++.
* | ``scan_full_initializer_expr_as_component``\ scans an initializer that need
    not be a constant.
* | ``scan_class_initializer_expression`` scans an initializer for a class.
* | ``scan_class_parenthesized_initializer`` scans a parenthesized initializer
    for a class.
* | ``scan_boolean_controlling_expression`` scans a scalar expression that
    controls a conditional statement (``if``, ``while``, ``do while``, and
    ``for``).

Each of these routines has roughly the same structure.  ``scan_expr_full`` is
called to do the actual expression scanning.  This produces a result of type
``an_operand``, which is checked for kind, converted to a required type (if
there is one), and then converted to the form required on return from the
interface routine, usually an expression node or a constant.

Some other utility expression routines with a different type of interface:

* | ``scan_ctor_arguments`` is called to scan a parenthesized list of
    expressions that acts as the argument list for a constructor call, for
    example

  .. code:: c++

     struct A {
       A(int, int);
     };
     A aa(1, j+1);  // scan_ctor_arguments is called

* | ``try_to_convert_class_operand_to_builtin_type`` is called to convert an
    expression of a class type to a builtin type, as when such an expression
    appears as the conditional expression in an ``if`` statement or the like.
* | ``scan_braced_init_list`` scans a complete braced-enclosed initializer as
    in the C++11 "initializer lists" feature.  ``convert_initializer`` is used
    to convert initializers, in particular brace-enclosed initializers, to the
    type of the entity being initialized.
* | ``find_default_constructor`` finds a default constructor capable of
    initializing an object of a given class type.
* | ``find_copy_constructor`` finds a copy constructor capable of copying an
    object of a given class type with a given cv-qualification.
* | ``find_copy_assignment_operator`` is similar for an ``operator=`` function.

``scan_expr_full``
------------------

``scan_expr_full`` actually handles all expression scanning.  It is called with
a set of option flags that configure its processing for various types of
expressions.  It scans through the tokens of an expression, builds an
expression tree for the expression scanned, and returns an argument of type
``an_operand`` to describe the expression scanned.

Its arguments are as follows:

.. list-table::

   * - | ``result``
     - | The result (of type ``an_operand``) of this level of expression
         scanning.
   * - | ``prec_level``
     - | The operator precedence level at which to terminate the scanning.
         Expression scanning continues while the next operator precedence is
         higher than this (or equal, with right-associative operators).
   * - | ``expression_kind``
     - | The kind of expression that is expected: ``ek_pp`` for a preprocessing
         expression, ``ek_integral_constant`` for an integral constant
         expression, ``ek_init_constant`` for a constant initializer expression
         (not used in C++, except for nontype template arguments and some C++11
         expressions), ``ek_normal`` for a normal expression, or ``ek_sizeof``
         for the operand of ``sizeof``.  For the three constant expression
         cases, some operators are restricted, some operand types are
         restricted, and values of variables (etc.) may not be loaded.  (See
         the section on constant expressions.)
   * - | ``local_options``
     - | A bit-set of option switches that apply to just this level of
         expression scanning (i.e., they do not apply to subexpressions scanned
         within this expression scan).  The switches control whether the comma
         operator is allowed, whether the expression is being scanned as the
         immediate operand of a cast, etc.

Note that typically ``scan_expr_full`` is called through the macro
``scan_expr``.

Parsing Technique
-----------------

``scan_expr_full`` scans an expression using a modified version of recursive
descent.  The technique is "modified" in that it includes an operator
precedence level (higher precedences, numerically, bind more tightly).  This
modification avoids the many levels of subroutine calls inherent in the
traditional recursive descent method.  In the modified method, there is
approximately one subroutine call per leaf operand.  Basically,
``scan_expr_full`` will scan an expression up to either something it does not
understand (presumably something that follows the end of the expression), or up
to an operator that has lower precedence than the level being scanned (or equal
precedence when left-associative).

The precedence check is done by the routine ``token_ends_expr``.  One special
case: when inside a template argument list, "``>``" is treated as the closing
bracket for the argument list rather than the "greater than" operator.

For example, the expression ``1+2*3-4`` is parsed as follows:

#. ``scan_expr_full`` is called with level 0. It takes the ``1``, then
   looks at the operator ``+``. The current level (0) is less than the
   level of ``+`` (12), so ``scan_expr_full`` takes the ``+`` and calls
   itself with level 12.

#. ``scan_expr_full`` (called with level 12) takes the ``2``, then looks at
   the operator ``*``. The current level (12) is less than the level of
   ``*`` (13), so ``scan_expr_full`` takes the ``*`` and calls itself
   with level 13.

#. ``scan_expr_full`` (called with level 13) takes the ``3``, then looks at
   the operator ``-``. The current level (13) is greater than the level of
   ``-`` (12), so ``scan_expr_full`` returns the constant expression for
   ``3``.

#. Back in ``scan_expr_full`` (level 12, step 2 above), ``2*3`` is
   assembled (by computing the constant result). ``scan_expr_full`` then
   looks at the operator ``-``. The current level (12) is equal to the
   level of ``-``, and ``-`` is left-associative, so ``scan_expr_full``
   returns the constant expression for ``2*3`` (i.e., 6).

#. Back in ``scan_expr_full`` (level 0, step 1), ``1+(2*3)`` is assembled
   (by computing the constant result). ``scan_ \expr_full`` then looks at
   the operator ``-``. The current level (0) is less than the level of
   ``-`` (12), so ``scan_expr_full`` takes the ``-`` and calls itself with
   level 12.

#. ``scan_expr_full`` (called with level 12) takes the ``4``, then looks at
   the next token of input, which is something unrecognized, and returns
   the constant expression for ``4``.

#. Back in ``scan_expr_full`` (level 0, step 5), ``(1+(2*3))-4`` is
   assembled (by computing the constant result).  ``scan_expr_full`` then
   looks at the next token of input, which is something unrecognized, and
   returns the constant expression for ``(1+(2*3))-4`` (i.e., 3).

``scan_expr_full`` Processing
-----------------------------

The processing within ``scan_expr_full`` is only slightly more complicated than
the description above.  There are two major sections: In the first, a leaf
operand (identifier or constant), a prefix operator followed by an expression,
or an expression in parentheses (or a cast) is scanned.  In the second section,
there is a loop with the above-described precedence level test at the top.  So
long as the next thing in the input (whether it is a postfix operator,
subscript, function argument list, or binary operator) has an appropriate
precedence, it will be taken and added to the current expression.  The loop
stops on the first thing that is not recognized or has a lower precedence.  A
loop is required for expressions containing equal-precedence left-associative
operators, like ``1+2*3*4+5``: the ``*3`` and ``*4`` would be accumulated on
successive iterations of the loop in one call of ``scan_expr_full``.

.. _scanning-idents:

Scanning Identifiers
--------------------

When ``scan_expr_full`` scans an identifier, it calls ``scan_identifier``,
which builds an operand for identifiers for enumeration constants, variables,
functions, etc.  Odd C++ "names" like ``operator+`` are also handled by
``scan_identifier``.  The appropriate routines are called to turn them into
pseudo-identifier tokens, and thereafter the handling is the same as for other
names.

In constant expressions, variables are only allowed if they can legitimately be
part of a constant expression.

For undefined identifiers in C mode, ``scan_identifier`` enters the identifier
as an undefined identifier; shortly, the identifier will turn out to be either
truly undefined (in which case an error is generated) or will turn out to be
the name of an implicitly-declared function (in which case
``decl_default_function`` in ``decls.c`` will be called to declare it as such).

In C++, nonstatic data members and nonstatic member functions are processed as
if preceded by "``this->``" (see ``make_this_pointer_operand``).  If the
identifier is a type identifier and it is followed by a left parenthesis,
``scan_functional_notation_type_conversion`` is called to scan a type cast.

Also in C++, special considerations are made for uses of identifiers in local
classes (which can only use entities from the enclosing function in very
limited ways) and in local lambdas (which have similarities to local classes,
with the added wrinkle that references to enclosing local variables must be
interpreted as uses of fields of the associated closure class, and these fields
may have to be created as a result of these references).  See
``bad_nested_function_variable_ref`` and
``make_selection_for_captured_variable``.

Scanning Constants
------------------

When ``scan_expr_full`` scans a constant, it calls ``make_constant_operand`` to
build the appropriate operand.  Floating constants are allowed in integral
constant expressions only as the immediate operand of a cast (see
``local_options``, above).  For string literals,
``make_string_constant_operand`` is called instead.  It produces an lvalue for
the array of characters; except in unusual cases (e.g., when the string is used
to initialize a character array), this is changed to a pointer to the string
(by ``conv_array_operand_to_pointer_operand``).

.. _scanning-operations:

Scanning Operations
-------------------

Processing for operators is done by lower-level routines.  The routines are
called when the operator token has been scanned, so for non-unary operators,
this means after the left operand expression has been scanned.  Most of these
routines have a structure similar to the following, which describes a generic
binary operation routine:

* | A check is made to see if the operator is allowed in the present type of
    expression (some operators, for example, are disallowed in some kinds of
    constant expressions).
* | The right operand is scanned by calling ``scan_expr_full`` with appropriate
    switches.
* | In C++ mode if either of the operands has a class type,
    ``check_for_operator_overloading`` is called to see if operator overloading
    applies.  If it does, an appropriate operator function call is generated.
    If not, the remaining steps are done.
* | The left operand (passed in) is converted from a glvalue to a prvalue.  In
    cases where the left operand is supposed to be an lvalue, that is checked,
    and ``using_lvalue`` or ``modifying_lvalue`` (depending on how the lvalue
    is used) is called.
* | The type of the left operand is checked.
* | The right operand is converted from a glvalue to a prvalue.
* | The type of the right operand is checked.
* | The types of the operands are checked to see if they are compatible with
    one another and with the operation.  This sometimes selects a subcase of
    the operation (e.g., pointer addition instead of integer addition).

  * | For arithmetic operations, ``determine_arithmetic_conversions`` is called
      to determine the usual arithmetic conversions and
      ``change_binary_operand_types`` is called to promote the operand types as
      necessary.
  * | For pointer operands, ``check_compatibility_of_pointer_operands`` is
      called to check the operand types and do implicit type changes.
  * | For pointers to members,
      ``check_ptr_to_member_operands_for_compatibility`` is called to do a
      similar check.
* | ``which_binary_operator`` is called to select the proper intermediate
    language operator for the operation on this type of operands.
* | ``do_binary_operation`` is called to produce the result operand, either by
    folding an operation on constants (by calling ``binary_operation`` in
    ``folding.c``, but not when not-evaluated), or by building an expression
    tree.
* | The start position of the expression is recorded in the result operand.

The prefix ``++`` and ``--`` operators are scanned by
``scan_prefix_incr_decr``.

The unary operator ``&`` is scanned by ``scan_ampersand_operator``.  It usually
calls ``take_address_of_lvalue`` to produce the prvalue address result.  In
``pcc`` mode, it issues a warning for taking the address of an array, and calls
``conv_array_operand_to_pointer_operand`` to produce address-of-array-element
instead of the ANSI address-of-array.  If the operand is the qualified name of
a class member, a pointer-to-member constant is created.  In C++/CLI, ``&``
applied to an lvalue that might be on the CLR heap (as determined by
``is_gc_lvalue_expr``) produces an ``interior_ptr`` result.

The C++/CLI unary operator ``%`` is scanned by
``scan_handle_address_operator``.  It produces an ``eok_handle_to`` or
``eok_box`` operator.  The result is a handle.

The unary operator ``*`` is scanned by ``scan_indirection_operator``, producing
an lvalue or function designator as a result.

``scan_arith_prefix_operator`` is called to scan the prefix operators ``+``,
``-``, ``~``, and ``!``.  The unary version of the code in
``do_binary_operation`` (fold if constant by calling ``unary_operation`` in
``folding.c``, else build expression node) is included directly here, since
these are all of the normal unary operators.

The ``sizeof`` operator is scanned by ``scan_sizeof_operator``, which calls
``is_decl_not_expr`` to decide whether or not the argument is a type name.  If
it is, ``type_name`` is called to scan the type.  If the argument is not a
type, it is scanned as a not-evaluated expression.  The size of the type is
taken from the type entry in either case.

``scan_alignof_operator`` is used to scan the ``__ALIGNOF__`` construct (an
extension; similar to ``sizeof``, but returns the alignment for a type instead
of its size).

``scan_intaddr_operator`` is used to scan the ``__INTADDR__`` construct (used
in the ``offsetof`` macro to scan an address expression and cast it to integral
type).

``scan_noexcept_operator`` scans the C++11 ``noexcept`` operator.

``scan_typeid`` scans the ``typeid`` operator.  It produces an expression node
of kind ``enk_typeid``.

``scan_dynamic_cast_operator`` scans the ``dynamic_cast`` operator.  Cases that
require runtime type determination are rendered as ``eok_dynamic_cast``
expression nodes (or ``eok_ref_dynamic_cast``, for casts to reference types);
the other cases are turned into ordinary casts.

``scan_static_cast_operator`` scans ``static_cast``.

``scan_safe_cast_operator`` scans the C++/CLI ``safe_cast``.  It's essentially
the same as ``static_cast`` except that it allows additional runtime-checked
cases, which are found by ``process_runtime_checked_safe_cast``.

``scan_reinterpret_cast_operator`` scans ``reinterpret_cast``.

``scan_const_cast_operator`` scans ``const_cast``.

``scan_new_operator`` is used to scan the ``new`` operator.  The ``new``
operation is rendered as an ``enk_new_delete`` operator tagged as a "new",
whose supplement describes the operator ``new`` function to call plus
(optionally) some kind of initialization of the space allocated (for example, a
constructor call).  For the C++/CLI ``gcnew``, an ``enk_gcnew`` is used, and
the supplement for that includes additional information, including an
initializer.  For a ``gcnew`` of a C++/CLI array, the supplement contains both
information about the array bounds (either as explicitly provided, or deduced
from the number of elements in the initializer), and an aggregate initializer
for the array elements (the latter is scanned by ``scan_cli_array_init``).  For
delegate initializers, ``scan_delegate_initializer`` scans the initializer,
which requires hand-coded checking because the delegate class doesn't have
constructors that accurately cover the allowed initializers.

``scan_delete_operator`` is used to scan the ``delete`` operator.  The
``delete`` operation is rendered as an ``enk_new_delete`` operator tagged as a
"delete", whose supplement describes the operator ``delete`` function to call
plus (optionally) a destructor to call on the space being deallocated.

A left parenthesis can indicate a cast, a nested expression, or a C++17 fold
expression.  ``scan_cast_or_expr`` is called for all cases.  It uses
``is_decl_not_expr`` to decide whether or not the thing following the left
parenthesis is a type name.  If it is, ``type_name`` is called to scan the
type, ``cast_type_pre_check`` to check the legality of the type, and
``do_cast`` to check and process the cast.  For casts to void, an explicit
cast-to-void expression node is placed on top of the expression, as a special
marker for ``simplify_void_operand``, but no simplification is done at this
point.  In ``pcc`` mode, certain casts of lvalues leave an lvalue as the result
(see ``still_an_lvalue``).  If what is inside the parentheses is not a type,
``scan_expr_full`` is called to scan a nested expression or the first term of a
fold expression.  If it turns out to be a fold expression (or the left
parenthesis was followed by an ellipsis, which also implies a fold expression),
``scan_fold_expression`` is called to scan the fold expression in either
generic form (during the prototype instantiation of the enclosing template) or
expanded form (during real instantiations).

The postfix ``++`` and ``--`` operators are scanned by
``scan_postfix_incr_decr``.

Subscripting is scanned by ``scan_subscript_operator``.  The subscript is
represented by an ``eok_subscript`` operator.  Constant subscripts are checked
for legality by ``valid_node_if_subscript`` or by the constant-folding
routines.  When the first operand is a C++/CLI array, the contents of the ``[``
...  ``]`` are scanned as an expression list (no top-level comma operator is
allowed), and an ``eok_cli_subscript`` operator is used.  Default indexed
properties, indexed properties, and ``operator[]`` functions can all be used to
implement the subscripting operation.  Property rewrites are considered both
for the entire subscripted operation and for the first operand if it can be
converted to something subscriptable.

Function calls are scanned by ``scan_function_call``.  The function name is
entered as a default function if it is undefined (in C89 mode).  The argument
list is scanned, and each argument is cast or default-promoted (see
``prep_argument_operand`` and ``arg_default_promote_operand``) appropriately.
The arguments are checked against the applicable function prototype or, if the
called function has an old-style declaration with a body, against the declared
old-style parameters.  If the called function was declared with the pragma
``__printf_args`` or the pragma ``__scanf_args``, and the format string is a
constant, ``check_printf_scanf_arg`` is called for each argument following the
format string.  It checks that the type of the argument matches the type of the
corresponding formatting specifier, and issues a warning if not.
``assemble_function_call`` and ``make_function_call`` are called to put
together the actual function call.  If the function is a virtual function, a
different expression operator is used; the virtual call is suppressed if the
function was named with a qualified name or if the proper function to call can
be determined statically.  If the function is an overloaded function,
``scan_call_arguments`` just builds a list of entries of type
``an_arg_list_elem`` that describe the arguments scanned.  No checking can be
done on them until the specific function to be called is identified in overload
resolution.

The operators ``->`` and ``.`` are scanned by
``scan_field_selection_operator``.  After the right operand (a member name) is
scanned, the processing breaks apart into several cases:

* | For nonstatic data members (C fields), ``do_field_selection_operation`` is
    called; it in turn calls ``fold_field_selection`` to fold field selection
    relative to a constant address to produce another constant address.  The
    expression operator generated is ``eok_dot_field`` or
    ``eok_points_to_field``.
* | For nonstatic member functions, ``do_member_function_selection`` is called.
    It produces an operand for a bound function, i.e., a function bound to a
    selector object.
* | For static data members and static member functions,
    ``combine_unneeded_selector_with_operand`` is called, and it generates an
    ``eok_dot_static`` or ``eok_points_to_static`` operation.

For nonstatic members, ``cast_pointer_for_field_selection`` is called to cast
the left operand pointer to a base class if necessary.

The operators ``->*`` and ``.*`` are scanned by
``scan_ptr_to_member_operator``.  For member function cases, the result is a
bound member function.  For data member cases, an ``eok_pm_field or
eok_pm_points_to_field`` operation is generated.

The operators ``*``, ``/``, and ``%`` are scanned by ``scan_mult_operator``.

The operators ``+`` and ``-`` are scanned by ``scan_add_operator``.

The operators ``>>`` and ``<<`` are scanned by ``scan_shift_operator``.

The operators ``>``, ``<``, ``<=``, and ``>=`` are scanned by
``scan_rel_operator``, and the operators ``==`` and ``!=`` are scanned by
``scan_eq_operator``.

The operators ``&``, ``^``, and ``|`` are scanned by ``scan_bit_operator``.

The operators ``&&`` and ``||`` are scanned by ``scan_logical_operator``, and
the operator ``?`` (and the associated ``:``) is scanned by
``scan_conditional_operator``.  When these operators have constant operands for
the boolean controlling expressions, some operands will be scanned as
not-evaluated expressions, and the result tree is (conditionally) simplified
accordingly.

The operator "``,``" is scanned by ``scan_comma_operator``.  Note that the
comma operator is not allowed at the top level in initializer expressions and
in argument lists, because it means something else in those contexts; see the
``local_options`` parameter of ``scan_expr_full``.

``scan_simple_assignment_operator`` scans the operator ``=``.  It calls
``prep_assignment_operand`` to check and convert the right operand in most
cases.  Assignment to "``this``" is recognized as a special case and
allowed with a warning.

``scan_compound_assignment_operator`` scans compound assignment operators
(``+=``, ``-=``, et al.).

Lambda expressions are scanned by ``scan_lambda_expression``.  The actual
parsing is handed to ``scan_lambda`` in ``class_decl.c``, and
``scan_lambda_expression`` constructs the required temporary initialization
from the resulting ``a_lambda`` entry.  (See also :ref:`scanning-idents` for
special measures needed when parsing identifiers in a lambda body.)

.. _expr-operands:

Operand Data Structure
----------------------

Throughout the expression routines (and nowhere else), all the information
about an operand is maintained in an entry called ``an_operand``.  The operand
contains the type of the operand, its "kind" (indicating the way in which the
operand is represented, i.e., as a constant or an expression tree), its "state"
(none, glvalue, prvalue, or function designator), and its source position.
Variables of type ``an_operand`` are typically stack-based variables within the
expression routines.  They are never allocated in the intermediate language
memory region.

An *lvalue* is an object in memory; an *rvalue* is a value, with no associated
memory location.  The distinction between lvalues and rvalues is very important
in the C language.  [#f1]_ Names of variables start out as lvalues, and if
their values are used, they are implicitly converted to rvalues.  C++11 adds
*xvalues*, which are eXpiring values produced by certain rvalue reference
operations.  They share an operand state with lvalues, since the two are very
similar (they have addresses, possibly cv-qualified types, dynamic type, object
identity).  They can be distinguished with the ``is_an_lvalue()`` and
``is_an_xvalue()`` tests.  Lvalues and xvalues are collectively known as
*glvalues*.  The things formerly known as rvalues in C and in pre-C++11 C++ are
now known in C++11 as *prvalues*, and prvalues and xvalues collectively are now
known as rvalues.

The expression scanning routines attempt to model the C++11 concept of "value
categories" exactly, and the ``an_operand`` entry plays a part in that.  [#f2]_
It is always clear from the operand whether an expression is currently an
lvalue, xvalue, or prvalue, and the transformation from glvalue to prvalue is
done deliberately and when appropriate (see ``conv_glvalue_to_prvalue``).

For functions, the equivalent of an lvalue is a *function designator*, and
there is a representation for those in ``an_operand``.

In the C++ language definition, the term "lvalue" includes C's function
designator as well as C's lvalue, but in this implementation and this
documentation we use the term with the narrower C meaning.

When the operand representation is an expression, the operand points to an
expression tree in an intermediate language memory region.  When the operand
representation is a constant, however, the constant is contained directly in
the operand (it is not allocated).  This allows the expression routines to scan
constant expressions and fold them to constant results without allocating and
throwing away useless intermediate constant entries.

When an operand represents a bound function, that is, a C++ member function
with an associated object, the operand has the ``bound_function`` flag set.  A
second operand is used to describe the associated object.  There's no direct
link between the two; the expression routines must carry around two operand
entries.

When expressions appear in a comma-separated list, such as an argument list,
the operands on the list are often represented as entries of type
``an_arg_list_elem``.  Such entries can represent either an expression (in
``an_operand`` form), or a brace-enclosed list, the latter being needed for the
C++11 "initializer lists" feature.

The C++/CLI ECMA standard mentions the concept of a "gc-lvalue," and defines
conversions between normal lvalues and gc-lvalues.  It turns out those concepts
are not needed, at least not in the suggested way, and no added kind of lvalue
is needed in ``an_operand`` entries.  The important principle is that the
language must never allow the address of something that might be on the CLR
heap to be placed in a normal pointer where the garbage collector cannot find
it.  So, whenever the address of something that might be in the CLR heap is
taken, the address must be placed in an ``interior_ptr`` or a ``pin_ptr``, both
which are visible to the garbage collector.  The functions
``is_gc_lvalue_expr`` and ``is_gc_lvalue_operand`` test for lvalues that might
be in the CLR heap; if they return TRUE, the address of such an entity is made
an ``interior_ptr``.

Primitive Operations on Operands
--------------------------------

Many routines exist to do primitive operations on operands.  Routines that
perform initialization are

* | ``set_operand_kind`` and
* | ``clear_operand``.

Constructors for the various kinds of operands are

* | ``make_constant_operand``,
* | ``make_string_constant_operand``,
* | ``make_integer_constant_operand``,
* | ``make_expression_operand``,
* | ``make_glvalue_expression_operand``,
* | ``make_indefinite_function_operand``,
* | ``make_sym_for_ptr_to_member_operand``,
* | ``make_lvalue_variable_operand``,
* | ``make_ptr_to_member_constant_operand``,
* | ``make_function_designator_operand``, and
* | ``make_field_operand``.

Routines that create operands for one- and two-operand expressions are

* | ``build_unary_result_operand`` and
* | ``build_binary_result_operand``.

Error operands are created for parts of expressions containing errors by

* | ``make_error_operand``,
* | ``conv_to_error_operand``,
* | ``error_in_operand``, and
* | ``error_and_make_error_operand``,

which differ in whether or not they issue errors, and where.
``make_node_from_operand`` converts an operand to an equivalent expression
tree.

Checking Operands
-----------------

``op_is_zero_constant`` tests for a zero operand.  There is also a set of
``check_...`` routines, which are called to verify that an operand has certain
attributes, and if not, issue an error and change the operand to an error
operand:

* | ``check_modifiable_lvalue_operand``,
* | ``check_integral_operand``,
* | ``check_integral_or_enum_operand``,
* | ``check_integral_or_enum_or_fixed_point_operand``,
* | ``check_arithmetic_operand``,
* | ``check_pointer_operand``,
* | ``check_object_pointer_operand``,
* | ``check_function_pointer_operand``,
* | ``check_scalar_operand``.

``check_boolean_controlling_expression`` checks that an expression controlling
a conditional is a scalar expression, and in C++ modes that don't disable the
``bool`` keyword, it converts the expression to ``bool``.

Transformations on Operands
---------------------------

Several routines handle transformations on expressions defined by the standard:

* | ``conv_glvalue_to_prvalue`` changes a glvalue to a prvalue.  Several
    special cases are handled.  Type qualifiers on the glvalue type are removed
    to make the prvalue type.  Most of the work for expression cases is handled
    by ``conv_glvalue_expr_to_prvalue``.  ``conv_glvalue_to_prvalue`` calls
    ``using_lvalue`` to do the error check for using the element just past the
    end of an array; it also generates an error if it is called to convert a
    glvalue in a constant expression.
* | ``conv_array_operand_to_pointer_operand`` changes an array to a pointer to
    the first element of the array.
* | ``conv_function_designator_to_ptr_to_function`` changes a function
    designator to a pointer to the function.

``do_operand_transformations`` is a convenient way to request the preceding
three transformations.  It can do any or all of them.

Note that in C++ operand transformations often cannot be done until one knows
the way in which an expression will be used.  In an expression like ``arr +
xx``, for example, with ``arr`` an array, one cannot convert the array to a
pointer until one has ruled out the possibility that this operation is an
overloaded use of operator "``+``" with a function for which the first
parameter is a reference to an array.  In C, the transformations can often be
done earlier, but for ease of handling they are delayed as in C++.

* | ``promote_operand`` handles the integral promotions (C standard, 6.3.1.1);
    these are done only where explicitly called for.
* | ``determine_arithmetic_conversions`` handles the usual arithmetic
    conversions (C standard, 6.3.1.8).  In ``pcc`` mode, the promotions are
    unsigned preserving (e.g., ``unsigned char`` is promoted to ``unsigned
    int``), and all ``float`` operations are forced to ``double``.  To
    determine integral promotion types ``type_after_integral_promotions`` (in
    ``types.c``) is called; a special case involving bit-fields is handled by
    ``type_after_bit_field_integral_promotion``.
* | ``arg_default_promote_operand`` does the default argument promotions on an
    operand (C standard, 6.5.2.2).
* | ``take_address_of_lvalue`` converts an lvalue to a prvalue pointer to the
    object.  This is the ``&`` operator for objects.  It checks for taking the
    address of a bit field or a register variable.
    ``take_reference_to_operand`` does the similar operation for a reference
    binding.

When fixed-point types (an Embedded C extension described in ISO/IEC TR 18037)
are enabled, transformations of fixed-point operands are also performed as
needed.  For example, in arithmetic operations involving both a fixed-point and
a floating-point operand the fixed-point operand is converted to a
floating-point type.  However, unlike most other mixed-type operations,
operations involving a mix of fixed-point and integer operands do not cause the
conversion of the operands to a common type.

The Expression Stack
--------------------

While expressions are being scanned, the expression routines maintain an
expression stack.  This stack snakes through local stack frames (i.e., it is
not allocated via ``malloc``).  The expression stack contains information on
the kind of expression being scanned, whether or not the expression is being
evaluated (e.g., the operand of a ``sizeof`` is not evaluated), and some other
minor information.

There is a new stack entry for each major kind of expression scanned, but not
for each level in the expression.  That is, if one started on a normal
expression, an entry would be placed on the stack; no new entries would be
added for parentheses or grouped operators in the expression; but if a sizeof
expression, or an integral constant expression in an array size in a type in a
cast, is reached, a new entry would be placed on the top of the stack.

See ``push_expr_stack`` and ``pop_expr_stack``.

Expression Lists and Initializer Lists
--------------------------------------

The C++ language, as of the C++11 revision, uses lists of expressions in two
contexts:

* | argument lists of function calls (syntax term *expression-list*), for
    example "``f(x, 2)``", and
* | initializer lists (syntax term *initializer-list*), for example
    initializers of variables as in "``A a{x, 2};``".

Those contexts at first glance might seem to have little in common other than
the fact that they involve lists of expressions.  However, the standard ties
the use of variadic template pack expansions and the use of brace-enclosed
initializer lists to those expression-list contexts, and in some cases
initializer lists become argument lists for constructor calls.  So, in the end,
it becomes appropriate and desirable to represent both kinds of expression
lists with the same data structure and to process them with many of the same
routines.

The data structure used is ``an_init_component``, which can represent

* | an expression, e.g., "``x``", in ``an_operand`` form, or
* | a braced-enclosed list, e.g., "``{1, 2, 3}``", as a pointer to a list of
    init-component entries, or
* | a designator, e.g., an array element designation like "``[1]=``" in an
    aggregate initializer, or
* | a continuation state to resume parsing when parsing of an initializer list
    was suspended (see below).

In initializer processing, e.g., in ``decl_inits.c`` and expression routines
that interface with that, the name ``an_init_component`` is used.  In routines
that are more related to argument lists and overload resolution, on the other
hand, the name ``an_arg_list_elem`` is used instead.  It's a typedef to the
same type, but it reflects a slightly different view of what the structure
represents.  In practice, the line between the two is a bit fuzzy, and
especially with some lower-level routines the naming has to choose one or the
other name and is therefore somewhat arbitrary.  Probably the main thing to
remember is that both names are used for the same data structure, with the
expression routines using the ``an_arg_list_elem`` name somewhat more often,
and the initializer routines using ``an_init_component`` exclusively.  About
the only real difference that can be pointed out is that an argument list
referred to via ``an_arg_list_elem`` will never have a designator on the list.

Init-component entries are linked together into lists and trees.  Something
like "``{1, {2, 3, 4}}``", for example, creates a braced-init-list component
pointing to a list of two entries, the second of which is a braced-init-list
component pointing to a list of three expression components.

The essence of the initializer lists feature in the language is that the same
notation can be used for initializations that are ultimately interpreted in
many different ways.  Therefore, the handling of initializer lists inherently
involves a two-step process: first, parsing the source into a tree of
init-component entries, and then interpreting those entries by context as
describing calls and initializations of various kinds.  At the simplest level,
the initializer processing calls ``scan_braced_init_list`` first to scan an
entire braced-init-list, and then ``convert_initializer`` to convert it to the
type of the entity being initialized.  The real work in scanning
braced-init-lists is done by ``parse_braced_init_list_full`` and its
subroutines.

In some cases the front end will process a very long braced initializer list in
parts to avoid tying up too much memory in init-component entries (which are
transient front end structures).  It does this by suspending and resuming the
parsing of the initializer (repeatedly if needed) and using placeholder
init-component entries (of kind ``ick_continued``) that point to some state
information sufficient to resume parsing later on.  This is fairly transparent
to the routines processing init-component entries, but requires that those
routines use macros like ``next_elem`` and ``is_last_elem`` rather than
directly access the ``next`` pointers of init-component entries.  This is only
possible for traditional aggregate initializers, but it is a worthwhile
optimization because occasionally programs contain initializers with thousands
or even millions of elements.

So some expression routines deal with an init-component (or list or tree of
them) as the "source" for an initialization.  And some routines will scan an
expression or braced-init-list in init-component form to do lookahead, and then
place the init-component entry in an *initializer cache*, later to be fetched
out of there as if it had been freshly scanned from source.  That's used, for
example, for declarations with "``auto``" type; an expression is scanned so its
type can be used for the ``auto`` type deduction, and the expression is then
put into the initializer cache so it will be picked up at the appropriate time
later.

Because expression components are scanned at one point and then revisited later
for processing, some of the context information for them must be saved along
with the init-component entries.  In particular, because it's not known at the
time of scan where the full-expression boundaries will fall, each expression is
scanned in its own invented lifetime, and is "bundled" with that lifetime in
the init-component.  When the expression is removed from the list later for
additional processing, the lifetime is reactivated or subsumed into an existing
lifetime.  Likewise, the cross reference entries associated with an expression
are detached from the current expression and saved in the init-component, and
then reactivated when the expression is processed further.  This allows things
like tracking of whether a variable is ultimately used as an lvalue or rvalue.
(See ``scan_expr_as_init_component`` for the bundling code, and
``extract_operand_from_expression_component`` and
``unbundle_init_component_expressions`` for the unbundling.)

The processing when a braced-init-list meets a destination type is handled by
``convert_initializer`` (callable from outside the expression routines) and
``prep_list_initializer`` (callable only from within the expression routines).
The latter does the real work.  It has some significant subroutines:

* | ``make_initializer_list_object`` makes an object of type
    ``std::initializer_list<T>`` from a braced-init-list, which involves a call
    to a constructor of ``initializer_list`` passing an array containing the
    element values.
* | ``value_initialization`` handles value-initialization, which is usually the
    result when the braced-init-list is an empty list, "``{}``".
* | ``check_narrowing_conversion`` checks for the "narrowing" conversions,
    issuing appropriate diagnostics.

``prep_list_initializer`` operates in one of three modes:

#. Producing ``an_operand result``. This is the usual mode for internal calls
   in the expression routines.

#. Guided by ``an_init_state``, producing either a dynamic init entry or a
   constant as a result. This is the usual mode for calls that come from
   initializer processing by way of ``convert_initializer``. This mode can
   be modified to produce no diagnostics or generate no IL, which is used
   for SFINAE analysis.

#. Doing overload resolution analysis, producing a result in an entry of
   type ``an_arg_match_summary``. That is used when evaluating arguments in
   overload resolution, and in that mode no errors are issued, no IL is
   generated, and the source init-components are not modified (in other
   words, it's purely exploratory).

All processing for aggregates, i.e., arrays and aggregate classes, is done by
code in ``decl_inits.c``.  ``prep_list_initializer`` calls
``prep_aggr_initializer`` to handle aggregate initialization from a
braced-init-list, and the initializer processing routines will call back into
the expression routines as necessary.  In certain cases, processing will hop
back and forth between the two areas repeatedly.  In those cases, the
``an_init_state`` structure is used to convey information across the
transitions, e.g., to record something that expression processing knows so that
as the transfer or control goes to the ``decl_inits.c`` routines and back into
expression processing that information is still known.

The expression routines have to be quite careful about whether they are dealing
with a single expression or (a member of) an expression list, and that is made
easier by using different structures to represent those, i.e., ``an_operand``
for a single expression, and ``an_arg_list_elem`` for (a member of) an
expression list.  There are some rare cases in the C++11 language where a
brace-enclosed list is allowed in a context where a single expression is
allowed, and in those cases ``an_operand`` of kind ``ok_braced_init_list`` is
constructed, which points to an init-component list.  That allows a lot of the
existing routines to accept a braced-init-list in some limited cases.  To give
one example, the assignment operators allow a braced-init-list as their second
operand, even though that's not an expression list context.  So, if a
braced-init-list is present, it is scanned as an operand (see
``scan_braced_init_list_as_operand``) and passed around in that form to the
usual routines.  When it gets to the operator-overloading routine
``check_for_operator_overloading``, that routine is prepared to unpack that
operand and use the braced-init-list in overload resolution.  There are also
some cases when rescanning expressions (see below) where a braced-init-list
operand is constructed so that a braced-init-list can be returned through an
interface that is designed for single operand expressions.

The ``an_arg_operand`` data structure, which in the past was used for operands
in argument lists, is now reserved for a few cases where a single
non-expression-list expression needs to be returned to a caller outside the
expression routines, specifically for nontype template arguments values.

In most cases, there is no direct IL representation for initializer lists,
because when a braced-init-list meets a destination type it is converted to
some expression form that is no longer a braced-init-list.  (For example, it
might become a dynamic init for a constructor call, or a nonconstant aggregate
dynamic init.) However, in prototype instantiations of templates there may be
braced-init-lists that have not met a real destination type and therefore
cannot be interpreted yet, and those must remain in braced-init-list form.  For
those, the ``enk_braced_init_list`` expression node kind is used.  It points to
a list of expressions, some of which may themselves be ``enk_braced_init_list``
nodes.

Constant Expressions
--------------------

Constant expression processing has two fairly different modes:

#. In C mode, and in C++ mode prior to C++11, each constituent of an
   expression is checked immediately to see whether it is allowed in a
   constant expression. An error is issued if not. So, for example, the
   appearance of a ``throw`` operator in a constant expression draws an
   immediate error. Class-typed values are never allowed, and neither are
   user-defined conversions. This mode is referred to as the "traditional
   constant expression" mode (see, for example,
   ``curr_expr_kind_is_traditional_const``).

#. In C++11 (roughly, when ``constexpr_enabled`` is TRUE), what matters is
   whether the expression overall folds to a constant, regardless of the
   constituents of the expression. So, for example, "``1 ? 2 : i``" is a
   valid C++11 constant expression: the reference to "``i``" is not
   evaluated and does not rule out a constant expression.  Class-typed
   constant values (of "literal" type) are allowed, and so are user-defined
   conversions using ``constexpr`` functions. This mode is referred to as
   the constexpr mode. (A variation of that mode, known as "relaxed
   ``constexpr`` mode", is enabled in C++14 mode.  See also
   :ref:`constexpr-and-expr-folding`.)

In both modes, references to constant-valued variables and operations on
constant operands are folded as soon as they can be, [#f3]_ and a constant
value is passed up to the next level, so the basic processing is very much the
same.  The main differences are:

* | In traditional constant expressions, many operator-scan routines check
    immediately on entry whether the operator they handle is valid, and issue
    an error if not.  In C++11 constant expressions, the check is done by
    calling ``operator_not_allowed_in_cpp11_constant_expr`` (or
    ``construct_not_allowed_in_cpp11_constant_expr``), which issues no error in
    unevaluated parts of expressions.
* | Various routines that handle looking for user-defined conversions (e.g.,
    ``check_user_defined_conversions_for_cast``) allow user-defined conversions
    when ``constexpr_enabled`` is TRUE.
* | Calls, including constructor calls, can be folded to constants.  This is
    done by first building the normal IL for the call and then, if the function
    or constructor involved is ``constexpr``, seeing whether the call can be
    folded to a constant.  This is done by ``expr_fold_constexpr_call`` and
    ``expr_fold_constexpr_ctor``.  Implicit calls, like conversion function
    calls, can also be folded to constants.  If backing expressions are being
    recorded, the original IL for the call is recorded as the backing
    expression for the result constant.
* | Both modes require tracking, in non-constant expressions, of whether the
    expression makes use of anything not allowed in a constant expression.
    [#f4]_ In traditional constant expressions, the kinds of expressions ruled
    out are tracked at the ``an_operand`` level (see the
    ``ruled_out_expr_kinds`` set and the ``rule_out_expr_kinds`` routine), and
    for specific kinds of constant expressions (e.g., integral constant
    expressions).  In C++11 constant expressions, a single boolean flag called
    ``constant_expr_ruled_out`` is maintained in the expression stack, and
    there's effectively only one kind of constant expression.  (Note, however,
    that in C++11 mode some kinds of expressions are scanned as traditional
    constant expressions, e.g., preprocessor expressions and nontype template
    argument address expressions, because the C++11 language rules still impose
    many restrictions on the kinds of operators that can be used in them.)
  | In certain contexts, the rules about what is ruled out are relaxed
    slightly.  Specifically, in contexts that will later be subject to
    parameter substitution in ``constexpr`` call evaluation, a use of the value
    of a parameter variable does not rule the expression out as a potential
    constant expression (because in a real evaluation, the parameter might have
    a constant value).  Those contexts (the return expression of a
    ``constexpr`` function, the mem-initializers of a ``constexpr``
    constructor, and the field initializers of a literal class) are identified
    by ``in_potential_constant_constexpr_context``.
  | When init-components are used, expression processing is often done in two
    steps: first, the expression is scanned into an init-component, and then
    the init-component is converted to the final required type.  In those
    cases, the ``constant_expr_ruled_out`` flag is copied from the expression
    stack into the init-component, and then back from the init-component to the
    expression stack when the conversion processing is started.
* | In several contexts, the C++11 standard calls for a "converted constant
    expression" of a given type (for example, an array bound is a converted
    constant expression of type ``size_t``).  This is implemented by
    ``process_converted_constant_expression``.  The required type can be a
    specific type, like ``size_t``, or a general category of types, like
    integral types.  In traditional constant expressions, that routine just
    scans a constant expression and converts it as necessary, not allowing
    user-defined conversions.  In C++11 mode, it allows user-defined
    conversions but imposes a restriction on the types of implicit conversions
    allowed at the end (e.g., not "narrowing" conversions).  The C++11 rules
    are therefore broader overall but more restrictive in certain cases.  The
    set of allowed implicit conversions is defined by
    ``impl_converted_constant_expr_conversion_possible``.
* | ``constexpr`` calls and constructions in constant expressions produce a
    constant result directly, i.e., not a temporary initialized by a constant,
    but other temporaries (e.g., a temporary needed to bind "``const int &``"
    to an integer constant ``2``) are created as usual.  Processing within
    folding fetches the value out of the temporary if appropriate, or creates a
    special compile-time temporary using a ``ck_address/abk_temporary``
    constant that holds the necessary constant value.  Operations that cannot
    be converted to those forms are left as initialization of a temporary,
    which is considered non-constant.

.. _rescanning-exprs:

Rescanning Expressions
----------------------

The C++11 standard introduced some new rules for template deduction (see
`WG21 paper N2634
<https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2634.html>`__).
When a function template is considered in overload resolution, part of the
process is "template deduction," which is an attempt to find values for the
template parameters of that template that will specify a template instance
that is a viable function for the call.  The deduction rules require that
any expressions that appear in the template type be valid after
substitution of the deduced template arguments for the template parameters.
Whereas in older versions of the standard "valid" was defined by a specific
list of requirements, and only relatively simple expressions were allowed,
the C++11 rules now allow arbitrarily complex expressions inside
unevaluated contexts (e.g., ``sizeof`` and ``decltype``), and "valid" is
defined by the normal semantic rules of the language, except that access
checking is not done.  So, for example:

.. code:: c++

   template <class T> auto f(T p1, T p2) -> decltype(p1 + p2) {
     return p1 + p2;
   };
   struct A {
     const A & operator +(const A&);
   };
   A a1, a2;
   int main() {
     f(a1, a2);
   }

Here, the call is valid only if the expression "``p1+p2``" is valid, which is
true only if there is a valid meaning for the "``+``" operator, possibly found
by doing overload resolution as in this case.  Any error detected during this
process should cause only a failure of deduction for that template, and not a
hard error.  (This idea is summarized in the acronym that is often used to
refer to this non-error-producing deduction process, SFINAE, which stands for
Substitution Failure Is Not An Error.)

To implement this, the Front End is capable of "rescanning" expressions to redo
the semantic checking on them.  An expression saved during the initial scan of
a template is copied, with substitution of template argument values for
template parameters, and is then passed through the normal
expression-processing routines in order to do everything that would have been
done to that expression after scanning it from source tokens.  A key part of
the infrastructure for performing that is that almost all operator-scanning
routines have an ``rcblock`` parameter, which can be used to pass in a rescan
control block.  When that parameter is non-NULL, it gives information about the
expression to be rescanned, the template arguments and template parameters, and
some options.  The operator scan function tests ``rcblock`` each time it is
about to get something from source tokens, and when ``rcblock`` is non-null it
instead uses the previously-scanned IL expression to produce the same
information in the same form.  That information then passes through the parts
of the operator routine that handle semantic checking, implicit conversions and
transformations, etc., undergoing exactly the processing it would have received
if scanned from source.

Because IL expressions don't contain all the information that was available
when the expression was originally scanned, additional information (in the form
of ``an_expr_rescan_info_entry``) is saved whenever an expression is scanned in
a context that might later be subject to a rescan (essentially, in function
template headers).  That rescan information includes a copy of the
``an_operand`` entry that represented the IL expression, and a bit of
additional information like the operator source position.

When deduction is being done, if ``expr_is_rescannable`` is true for an
expression, it is passed to ``rescan_expr_with_substitution_internal``, which
determines the operator-scanning function that corresponds to the top operator
of the expression (see ``operator_token_for_expr_rescan``) and then calls that
operator-scanning function.  The scanning function will then do substitution
and rescanning on its operands (see ``make_rescan_operands``), followed by the
semantic checking and processing it usually does, before returning an operand
back to its caller, which will continue through the rescan process.  If an
error occurs, the error flag in the rescan control block is set, and once the
processing returns to the top level the deduction fails.

The expression rescanning code co-exists with the older code that handled
SFINAE processing for many years before this change, e.g., routines like
``copy_template_param_expr``.  Those routines are still used when new-style
SFINAE is turned off (see ``cpp0x_sfinae_enabled``), and also for parts of
expressions that aren't rescannable.

For braced-init-lists, a copy of the original list in init-component form is
saved in the rescan info operand (see
``save_rescan_info_for_braced_init_list``).  On the rescan, that list is
rescanned.  As with all rescans, the source of the rescan is mined for
source-like information, and a lot of other information is ignored.  That
means, for rescanned braced-init-lists, that the original list is not modified
or really "used" -- it's a guide for the rescan as opposed to a source of
expression operands to be used directly.

Making rescanning work requires some special coding conventions in
expression-processing routines:

#. The error-reporting routines cannot be called directly. Instead the
   corresponding routines beginning with "``expr_``" must called. So, for
   example, instead of "``pos_error``" one must call
   "``expr_pos_error``". This allows the interception of error calls so
   that they set a flag in the expression stack instead of issuing an
   error.

#. Code that checks access must be conditional on
   ``expr_access_checking_should_be_done()``. Note that it's usually
   necessary to avoid the access checks altogether rather than doing them
   and getting to the point where an error would be issued, in order that
   complete and correct IL is generated.

#. Calls out to utility routines in other parts of the front end must not
   generate errors (unless those errors come from handling something
   outside of the expression; for example, it's okay to issue an error out
   of a template instantiation that is kicked off by the expression
   processing). If those utility routines might generate errors, they must
   have parameters that can be used to get an error return instead of
   having an error issued. For an example, see ``binary_operation`` in
   ``folding.c`` and its ``error_detected`` parameter.

#. Scanning routines for added operators must have an ``rcblock``
   parameter, to indicate the rescan. Within such routines, all processing
   that deals with source tokens must have a rescan alternative that pick
   up the information from the expression being rescanned. As a general
   guideline, beware of using ``curr_token``, ``pos_curr_token``,
   ``end_pos_curr_token``, ``locator_for_curr_id``,
   ``const_for_curr_token``, ``curr_construct_ end_position``,
   ``error_position``, and the error-reporting routines that use an implied
   position.

   Near the end of the scanning routine, call
   ``record_operator_position_in_rescan_info`` to record the operator
   position in the rescan information for the expression.

   The scanning routine should be added to
   ``operator_token_for_expr_rescan`` and
   ``rescan_expr_with_substitution_internal`` so it will be rescannable. If
   an operator scanning routine is not added to those routines, it will not
   be rescannable, and if it comes up in a deduction context the deduction
   will simply fail.  That may be acceptable for certain kinds of
   operators.

#. Note that none of these restrictions are necessary for features used
   only in C mode, but may be desirable anyway to allow the option of using
   that feature in C++ mode at some later date.

Void Expressions
----------------

When a void expression has been scanned, either by ``scan_void_expression`` or
as the first operand in ``scan_comma_operator``, ``simplify_void_operand`` is
called to trim off any parts of the expression that have no effect.  This
trimming is done very conservatively, however.  Top-level casts to ``void`` are
removed.  Other kinds of trimming could be done, but they are not in the
interest of preserving the source expression.  ``simplify_void_operand`` issues
a warning for expressions that have no overall effect.  (See
``node_has_side_effects``.)

Expressions that are explicitly cast to void are not processed as void
expressions at the time of the cast; a cast-to-void node is placed on top of
the expression, and the expression is handed up.  If the node makes it up to
the top level, it is simplified and the cast-to-void node is removed.  Casts to
void can remain in the final intermediate language, but only in rare cases
(such as

.. code:: c++

   i>0 ? (void)f(1) : (void)g(2)

as a statement).

Type Conversions
----------------

Type conversions happen implicitly (in assignments and initializations) and
explicitly (in casts).  Either way, they are done under control of the
expression routines.  And they must be: in C++, conversions involve not just
the type of an expression, but whether it is and stays an lvalue or an rvalue,
and whether or not other implicit transformations may be done.  The data
structure that can express these concepts is ``an_operand``, and it exists only
within ``expr.c``, ``exprutil.c``, and ``overload.c``.

To scan and process an expression correctly, one needs to know what will
ultimately be done with it, specifically what type it will be converted to.
Therefore, in all cases involving conversion, all necessary information about
the destination is passed into the expression routines, and they take care of
the conversion.  By the time the expression is returned to the caller, it has
the correct type.

The routines that control these conversions have names beginning with
"``prep_``":

* | ``prep_conversion_operand`` deals with the general assignment or
    initialization case.
* | ``prep_initializer_operand`` deals with initialization.  Its interesting
    special case is initialization of references.
* | ``prep_elision_initializer_operand`` deals with initialization of entities
    with class types that have constructors, where it may be possible to avoid
    a copy constructor call.
* | ``prep_argument_operand`` deals with initialization when it applies to
    arguments.  Its interesting special case is generating copy constructor
    calls for class values passed by value.
* | ``prep_return_by_cctor_operand`` deals with expressions in ``return``
    statements (also considered an initialization case), specifically in
    routines that return class values by calling a copy constructor.  A dynamic
    initialization entry describing the initializing operation is built and
    returned to the caller.
* | ``prep_assignment_operand`` deals with the right-side expressions in
    assignments.

In general, these routines do their work in two parts:

* | They determine whether or not the source operand can be converted to the
    destination type, and if so, how.  This is done by calling
    ``conversion_possible``.
* | If the conversion is valid, they do it, by calling ``convert_operand``.

``conversion_possible`` uses ``impl_conversion_possible`` (from ``types.c``) to
see if a standard conversion if available to do the conversion); if not, it
calls ``user_defined_conversion_possible`` to see if a user-defined conversion
can be done.

``user_defined_conversion_possible`` calls ``conversion_to_class_possible`` to
check for constructors and ``conversion_from_class_possible`` to check for
conversion functions that might be applicable.

``convert_operand`` calls ``cast_operand`` for simple conversions, and
``user_convert_operand`` for user-defined conversions.

``cast_operand`` and ``cast_node`` are called to change the type of an
operand or expression node.  (``cast_operand`` is actually just a wrapper
on top of ``cast_operand_full``, which does the real work.) The caller of
those routines must have already determined that the conversion is valid.
If the cast is implicit and if it does not change the type of an operand,
that operand is usually left alone.  (An exception are casts on bit fields,
because a bit-field access that has no cast on it has slightly different
properties -- especially with respect to promotion -- from the same access
with a cast on top.)

If the operand is a constant, ``type_change_constant`` is called to fold the
conversion.

If the operand is an expression, ``add_cast_to_node`` is called to add a cast
to the expression tree.  For casting a pointer-to-class to a
pointer-to-related-class it calls ``add_base_class_casts`` or
``add_derived_class_casts``, and for the similar cases involving pointers to
members, it calls ``add_pm_base_class_casts`` or
``add_pm_derived_class_casts``.

The routines that scan explicit casts use several lower-level routines:

* | ``check_user_defined_conversions_for_cast`` looks to see if a cast performs
    any user-defined conversions, and if so applies them.
* | ``set_up_for_cast_to_reference`` does some checking and setup in the cases
    where the cast is to a reference type.
* | ``cast_operand_for_reference_cast`` is like ``cast_operand``, but for cases
    where the operand is being cast to a reference type (including an rvalue
    reference type).
* | ``generic_cast_operand`` performs a cast when either the source operand or
    the destination type involves something template-dependent in a prototype
    instantiation.  In such cases, it's not generally possible to know what the
    cast does, and we just want to put a generic representation of it in the
    IL.

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
non-constant -- address.  However, MSVC treats such strings as constants, so
representing them that way makes it easier to emulate MSVC's behavior.  See
``literal_type_convertible_to_cli_string``,
``is_literal_convertible_to_cli_string``, and
``convert_operand_to_handle_to_cli_string``.

C++/CLI also allows boxing and unboxing conversions, i.e., between a value of a
value type and (a handle to) a boxed value in the CLR heap.  Boxing can be
either an implicit conversion or a cast; unboxing is always done explicitly by
a cast.  Operands having fundamental types will be implicitly converted to a
boxed value in cases where a class is required, e.g., in an example like
"``(3).toString()``", or when they are implicitly converted to an appropriate
handle type.  Significant routines for boxing and unboxing are
``is_boxable_type``, ``add_box_to_expression``, ``add_unbox_to_expression``,
``box_value_type_operand``, and ``unbox_after_indirection_if_required``.

Temporaries
-----------

``create_expr_temporary`` creates an ``enk_temp_init`` node that defines a
expression temporary.  It hangs a dynamic initialization entry under that node
and, if the temporary will require a destructor call, puts the destructor
information in the dynamic initialization entry.

``alloc_dtor_dynamic_init`` is the routine that actually allocates the dynamic
initialization entry for the temporary.  If the temporary is within the
expression in a return statement in a routine that returns its value via a copy
constructor, the destructor routine is recorded in the dynamic initialization
entry, but the destructor routine is not marked as referenced.  This is because
the topmost initialization in the return expression should not indicate a
destructor (the caller does the destruction), but one cannot know at the time
the dynamic initialization entry is created whether or not it will end up being
the topmost one.  Therefore, all entries are given destructors, but the
destructor routines are not marked as referenced.  Each such dynamic
initialization is placed on a fixup list.  After the entire expression is
processed, the destructor indication is cleared in the topmost initialization,
and ``fix_up_dynamic_init_dtors`` is called to revisit the dynamic
initialization entries and mark the (remaining) destructors as referenced.

``temp_init_from_operand`` creates a temporary variable and initializes it from
a given operand value.

``convert_operand_into_temp`` creates a temporary variable and initializes it
from a given operand value where there might be type conversion involved.

Note that the temporary closure object resulting from a lambda expression is
not represented with an ``enk_temp_init`` node; the ``enk_lambda`` node itself
represents the temporary (it points to an associated dynamic initialization
entry).

Copy Constructor Elision
------------------------

In cases where a class entity is being initialized, it is often possible to
avoid calling a copy constructor:

.. code:: c++

   struct A { A(int) {...}};
   A xx = 1;         // A::A(int)

The constructor in that case can be used to directly initialize the variable
``xx``.  It's not necessary to initialize a temporary with that constructor and
then copy the temporary to ``xx`` using a copy constructor.  The process of
avoiding the unnecessary copy constructor call is called *copy constructor
elision*.

``determine_dynamic_init_for_class_init`` is the routine that checks for the
possibility of copy constructor elision.  It attempts to find a constructor or
other routine that can do the initialization directly.  Failing that, it
returns a copy constructor.

If a copy constructor call is elided, the copy constructor must still exist.
To confirm its existence and accessibility ``handle_elided_copy_constructor``
is called.

Selector Operands
-----------------

A function call like

.. code:: c++

   p->f(xx);

is processed by (1) scanning ``p->f`` and generating a pair of operands (one
for the object, one for the function) bound together, then (2) processing the
call.  Between the first step and the second, the selector operand for the
object is carried around alongside of the (primary) operand for the function.

If the function being called is a nonstatic member function (which would be the
usual case), the selector must sometimes become a pointer to an object rather
than an lvalue or rvalue for the object.  ``conv_operand_to_object_pointer``
does that transformation.

``conv_selector_to_object_pointer`` also does that transformation but has a
flag indicating whether or not the object has been turned into a pointer, so
that the transformation is not done more than once.

``conv_object_pointer_to_lvalue`` does the transformation in the other
direction.

It is noteworthy that the addresses of class prvalues can be taken.  The
language does not allow the addresses of other kinds of prvalues to be taken.
However, the addresses of class objects are needed in order to do copy
constructor calls, member function calls, and base/derived class casts.  The
language hints that all class prvalues might really be temporary variables of
class type, and that therefore it is possible to take their addresses.
Accordingly, the front end generates temporaries for class prvalues, including
cases when functions return class values.  The routine
``conv_class_prvalue_operand_to_object_pointer`` can turn an expression for a
class prvalue back into a pointer to the class object, usually by finding the
``enk_temp_init`` node that defines the temporary and flipping the
address/value flag in that node back to "address."

Overloaded Function Calls
-------------------------

Overloaded function calls can be explicit calls or they can be implicit in
contexts that involve user-defined conversions (constructors or conversion
functions).  In either case, the process of overload resolution must be done.
It involves considering an argument list and a set of overloaded functions,
selecting the function that best matches the argument list, then adjusting the
types of the arguments so that they are appropriate arguments for the function
selected.

The argument list is represented as a list of entries of type
``an_arg_list_elem``, with each entry containing either an expression in
``an_operand`` form or a brace-enclosed list.

Each function in the overload set is considered in turn to see how well it
matches the argument list.  If all of the arguments can be made to match the
function parameters, the function is added to a candidate functions list (see
type ``a_candidate_function``).  Under that entry is a list of entries that
describes how well each argument matches the function's corresponding formal
parameter.

After all the functions are considered, ``select_best_candidate_functions`` is
called to select the function(s) with the best argument matches.  For each
argument, it forms the set of functions that match best on that argument; then
the intersection of those best-match sets is formed.  If the intersection has
just one function, it is the best-matching function.  However, it must still be
compared to all the other functions to verify that the chosen function is
better in some argument position than each of the other functions (though not
necessarily on the same argument position for each function).  After the
candidate functions list has been trimmed to a list of best-matching functions,

* | If there are no functions on the list, an error is issued: no function is
    appropriate.
* | If there is more than one function on the list, an error is issued: several
    functions are appropriate, so the call is ambiguous.
* | If there is exactly one function on the list, that is the function to call.
    The arguments are converted to the proper parameter types and the call is
    generated.

``compare_arg_match_levels`` compares two argument match summaries and
determines which of the two is a better match.  Often, this is just a matter of
comparing the match levels.  However, one of the overload resolution rules says
that if one match is a subsequence of the other (for certain cases), the
shorter sequence is the better match; this is the routine that implements that
rule.  When comparing two matches at the same level, it looks to see if either
one is a subsequence of the other, and if so, the shorter sequence is called a
better match.  The functions ``compare_argument_tiebreakers`` and
``compare_standard_conversions`` are called to do most of the work.

There are several tie-breakers that will select one function over another when
the argument matches are equally good for the two functions.  See
``compare_candidate_functions``.

If function templates are involved in the overload resolution, the candidate
functions set is built more or less as described above.  After an initial check
that the number of arguments matches the number of parameters,
``function_template_call_argument_deduction`` is called to do template
parameter deduction for the call.  If deduction is possible, it returns a list
of template argument values and a routine type with the template argument
values substituted for the corresponding template parameters.  This routine
type (which is a normal routine type containing no template parameters) is then
used for the rest of the overload resolution process.  If the function name was
accompanied by a set of explicit template arguments (e.g., ``f<int>(1)``), the
explicit template argument list is passed to the deduction routines and those
template parameters are not deduced.
``function_template_call_argument_deduction`` calls ``matches_template_type``
to do the actual deduction.  After the list of candidate functions has been
built, ``select_best_candidate_functions`` considers the template candidates
alongside the non-template functions.  If a template function is chosen as the
best function, ``find_template_function`` is called to build the instance of
the function, and the template function routine is then used as the result of
the overload resolution.

When template-dependent name lookup is enabled, some special processing is
done.  If, in a prototype instantiation, the argument list contains an
expression of template-dependent type, overload resolution cannot be done.  A
special "unknown dependent function" indication is returned to the caller.  If,
in a prototype instantiation, the argument list contains no dependent
expressions, the call is a "non-dependent call" according to the standard.
Overload resolution is done, and the function selected is recorded (in
association with the token sequence number of the position of the call or
overloaded operator) by calling ``record_nondependent_call``.  Then, in a real
instantiation of the template, ``get_nondependent_call_info`` is called to
retrieve the information, if any, recorded for a given call.  If there is
information associated with the call, the call is non-dependent and the
function previously determined by overload resolution is used again (overload
resolution is not done).

There are several things that are done during the processing of a
non-overloaded call that cannot be done at that point during the processing of
an overloaded function call, because it's not known which specific function
will be called.  Once overload resolution has been done, those things are done
by ``overloaded_function_catch_up``:

* | Check access to the function if it is a class member.
* | Record reference information for the function called.
* | Check that the return type of the function is not incomplete.

The casting of the arguments to the proper types is also a "catch-up" function.

``select_overloaded_function`` is the top-level routine for overload
resolution.  To find the best match it calls ``try_overloaded_function_match``,
which first calls ``determine_arg_match_level`` to see how well the arguments
match (for the selector object, it uses ``selector_match_with_this_param``
instead) and then calls ``select_best_candidate_functions`` to pick the best
function from the list of viable functions.

``set_up_overload_set_traversal``, ``next_symbol_in_overload_set``,
``set_up_overload_symbol_list_traversal``, and
``next_symbol_in_overload_symbol_list`` are used to set up and traverse
overload sets (the first two for normal sets, and the second two for sets
introduced by symbol lists, e.g., argument-dependent lookup or conversion
functions).

``determine_arg_match_level`` implements the argument matching rules of the
language.  An argument can match at any of the following levels:

.. list-table::

   * - | ``aml_exact``
     - | Exact match or trivial conversions.
   * - | ``aml_promotion``
     - | Match with promotions.
   * - | ``aml_std_conversion``
     - | Match with standard conversions.
   * - | ``aml_boxing_conversion``
     - | Match with boxing conversion (C++/CLI only)
   * - | ``aml_user_conversion``
     - | Match with user-defined conversions.
   * - | ``aml_ellipsis``
     - | Match with ellipsis.
   * - | ``aml_error``
     - | Match with error type (not in ARM).
   * - | ``aml_none``
     - | No match.

C++/CLI adds a few additional complexities:

* | Parameter arrays, where zero of more arguments at the end of the argument
    list can be bundled together into a CLI array of values passed to a final
    parameter with a parameter array type.  Such a match is worse than other
    matches except for an ellipsis match.  Parameter arrays are also handled
    when template type deduction is done.
* | Overload sets are formed using hide-by-sig lookup.
    ``use_hide_by_sig_lookup`` is called on a function symbol to see whether
    hide-by-sig lookup applies (it only applies to members of managed classes),
    and to build a list, attached to the symbol, of the symbols that should be
    in the overload set for that symbol.  The overload set traversal routines
    ``set_up_overload_set_traversal`` and ``next_symbol_in_overload_set``
    determine whether the symbol requires hide-by-sig lookup, and traverse the
    hide-by-sig list if so, skipping any inaccessible functions.  Since
    hide-by-sig applies only to member functions, it never has to co-exist with
    argument-dependent lookup.

User-defined Conversions
------------------------

``conversion_to_class_possible`` looks for a constructor or conversion function
that will convert an expression to a particular class type, and
``conversion_from_class_possible`` looks for a conversion function that will
convert an expression from a class type to a specific other type or to a
built-in type from a given set.  These use ``try_overloaded_function_match``
for the constructor cases, and ``try_conversion_function_match_full`` for the
conversion function cases.

Conversion functions that return reference types are a little tricky in that
their results are lvalues or xvalues, but that falls out more or less naturally
from the fact that operand transformations (like glvalue to prvalue) are put
off as long as possible.  When the result of a conversion is to be bound to a
reference, the kind of reference (lvalue reference or rvalue reference)
constrains the choice of suitable conversion functions.

C++/CLI adds static conversion functions.  Those can convert to a class in
addition to from a class, and can also convert to/from a handle to a class or
tracking reference to a class.  Therefore, places that look for conversion
functions must also look in the destination class for conversion functions, and
in the base classes of the source type, and must also consider handles to be
class-like as both source and destination types of a conversion, because the
classes underlying those handle types may have static conversion functions.
See ``try_static_conversion_function_match`` and
``cli_handle_user_defined_conversion_possible``.

Operator Overloading
--------------------

``check_for_operator_overloading`` looks for operator overloading
possibilities.  It is called only when one or more of the operands of an
operator has a class or enum type [#f5]_.  It considers three ways of
processing the operands:

#. As arguments of an operator member function.

#. As arguments of an operator function that is not a member function.

#. As operands of the built-in version of the operator, with the operands
   converted from the class types to acceptable built-in types by use of
   conversion functions. This is not tried for operators that have a
   built-in meaning for classes ("``,``", "``->``", "``=``", and unary
   "``&``").

As with overload resolution for function calls, the overloaded operator
resolution process considers each of the alternatives (functions and built-in
operator) in turn and builds a list of candidate functions.  After all the
alternatives have been examined, the best one is selected (or an error is
issued).  The operands are then adjusted to match the function or built-in
operator that was selected.

``try_overloaded_function_match`` is called (possibly several times) to
determine how well the operator functions match the operands.
``opname_member_function_symbol`` is used to find the applicable member
functions (if any), and ``nonmember_operator_function_lookup`` is used to find
the applicable nonmember functions (if any); special processing is done for
those if the operand types come from namespaces.

``try_conversions_for_builtin_operator`` is called to try to find conversions
that will allow use of the built-in version of the operator.  Its subroutines
-- ``operand_type_pattern_for_operator``, ``try_builtin_operands_match``, and
``try_pointer_builtin_operands_match`` -- do the necessary investigation.

C++/CLI adds some additional issues:

* | When the first operand is a handle, it is overloadable much as if it had
    the underlying class type.  An expression like "``h+x``" can be treated as
    "``h->operator+(x)``".
* | When either operand is a handle, it may be possible to use a static
    conversion function to convert the operand to a fundamental type for which
    there is a builtin version of the operator, much as is done for class
    operands.
* | Static operator functions can apply.  They don't have a "``this``"
    parameter, so the first operand matches the first parameter, and the
    second operand (if there is one) the second parameter.  "``a+b``" could
    become "``X::operator+(a,b)``".  These are also searched for in the
    class of the second operand, if there is one and it has class type.
* | Operator synthesis can be done.  If a class does not have an
    "``operator+=``", for example, the combination of an "``operator+``" and an
    "``operator=``" can be used.
* | In certain cases, string literals can act as if they are handles to
    ``System::String``, which then makes them overloadable.

Address of Overloaded Function
------------------------------

When an overloaded function appears in an expression, it is represented an as
indefinite function operand, which is basically just an operand that points to
the overloaded function symbol.

When such an operand is subjected to the function-to-pointer transformation,
the operand stays an indefinite function, but changes from a function
designator to a prvalue pointer.

When such an operand shows up in a conversion where the destination type is a
function pointer, ``find_addr_of_overloaded_function_match`` is called to find
out which (if any) of the specific functions matches the required function
pointer.  The operand is then converted to the specific function pointer in
``cast_operand_full``; ``overloaded_function_catch_up`` is called to do
whatever would have been done had it been known all along which specific
function was intended.

If the overload set includes one or more template functions, the resolution
algorithm is as follows:

#. If there is an exact match in the set of non-function-templates, it is
   selected (if there is more than one, the pointer use is ambiguous);
   otherwise,

#. If there is a template function from which a function can be generated
   that will match exactly, it is selected (if there is more than one, the
   pointer use is ambiguous); otherwise,

#. If there is a non-exact match in the set of non-function-templates, it is
   selected (this can only happen with pointers to member functions, where
   an implicit conversion from base to derived is allowed).

``matching_template_function`` is called to attempt to find a match under rule
(2).

Range-based ``for`` statement
-----------------------------

The C++11 range-based ``for`` statement is checked and assembled by
``check_range_based_for_statement``.  It determines which of the three
"patterns" is applicable for the statement and calls
``check_range_based_for_array_case``, ``check_range_based_for_member_case``, or
``check_range_based_for_default_case`` as necessary to perform the necessary
semantic checks and generate the IL.

The range-based-for statement takes the form:

 | ``for (`` *for-range-declaration* ``:`` *expression* ``)`` *statement*

and is defined to be equivalent to:

 | ``{``
 |    ``auto && __range = (``\ *expression*\ ``);``
 |    ``for ( auto __begin =`` *begin-expr*\ ``,``
 |               ``__end =`` *end-expr*\ ``;``
 |          ``__begin != __end;``
 |          ``++__begin ) {``
 |      *for-range-declaration* ``= *__begin;``
 |      *statement*
 |    ``}``
 | ``}``

Lower level routines for handling variable initializer expressions, member
lookup, overload resolution, etc.  are shared (where applicable) with the
for-each statement handling described below (together the range-based ``for``
and for-each statements are referred internally as "enhanced ``for``"
statements).

For-each Statement
------------------

The C++/CLI for-each statement is checked and assembled by
``check_for_each_statement``.  There are four "patterns" for the for-each
statement (the STL pattern, the CLI collection pattern, the CLI array pattern,
and the native array pattern).  ``check_for_each_statement`` determines the
pattern matched by a given loop, and calls the right one of four routines, each
charged with checking one of the patterns and building the IL.

The for-each statement is defined in terms of a rewrite for each of the
patterns.  For example, a for-each loop that matches the native array pattern
is rewritten from

 |   ``for each (``\ *T t* ``in`` *c*\ ``)`` *statement*

to

 |   ``{ C &cref =`` *c*\ ``;``
 |     ``I *cend = &cref[0]+``\ *c_num_elements*\ `;``
 |     ``I *i = cref;``
 |     ``for (; i != cend; ++i) {``
 |       *T t* ``= static_cast<``\ *T*\ ``>(*i);``
 |       *statement*
 |     ``}``
 |   ``}``

The IL for this case contains variables and associated scopes for the iterator
variable and the added temporary variables, initializer expressions for each of
those, and additional expressions for the loop ``!= and ++`` expressions.
Those are relatively simple in this case, but in other cases and other patterns
the generated expressions can be complex, and developing them may involve
member lookup, overload resolution, operator overloading, or user-defined
conversions.  The front end's goal is to provide IL that completely describes
the actions needed to implement the loop, while at the same time retaining the
parts in a high-enough-level form that source-analysis programs can discern the
original loop variables and expressions.

Properties
----------

Microsoft mode has properties, which are data members of classes that are
implemented by accessor functions, e.g., one to get the current value of the
property, and one to set the current value of the property.  Properties come in
two kinds:

* | Old-style properties, indicated by the ``__declspec(property(``...  ))
    attribute.  The accessor functions are only loosely associated with the
    property, by being named in the attribute.
* | New-style properties in C++/CLI, which begin with the "``property``"
    context-dependent keyword.  The accessor functions are declared as part of
    the property declaration, in a form that looks vaguely like a nested class.

In both cases, the IL has a description of the property declaration, in
something close to the source form (that information is held in
``an_property_or_event_descr`` entry).  However, also in both cases, references
to the properties are expanded by expression processing, so they are not
retained in source form.  A bit of source code like "``p``\ ``+=``\ ``1``"
might be expanded into a series of calls something like "call the get accessor
for ``p``, call ``operator+`` on the fetched value and 1, and call the put
accessor to store the computed value." The generated IL contains some tags on
various nodes in the expansion that help source-analysis code recover the
original meaning, though in a cumbersome way.

Property references cannot be rewritten immediately.  They have to be carried
around for a short while in expression processing until it becomes clear from
the context how the property is being used, i.e., a "get" or a "set".  Before
they are rewritten, property references are carried around as ``an_operand``
entries of kind ``ok_property_ref``.  The operand can include a list of
subscripts that is part of the property reference.  Once the context is
established, the property reference is rewritten; that's handled by
``rewrite_property_reference``.  As hinted at above, some rewrites are more
complicated than a simple "get" or "set", and might involve a "get", some
operation on the fetched value, possibly surrounded by conversions, and finally
a "set".  That happens for compound assignment operators and for pre- and
post-increment and -decrement operators.  In such cases, the operand is cloned
(see ``clone_operand``), so that it will only be evaluated once, and
``rewrite_property_reference`` is called twice, once for the "get" and once for
the "set".

C++/CLI events are very similar to the new-style properties, in representation
and in the fact that references are expanded by the front end.

C++/CLI Generics
----------------

Expression processing for C++/CLI generics, meaning handling of expressions
during the scanning of a generic definition, mostly falls out of normal
expression processing.  During the generic definition, the parameters of the
generic (represented as template parameters) are given values that are
generated class types that reflect the constraints on the generic parameters.
So, for example, if a generic parameter is constrained to derive from an
interface I, the generated class type will be such that it will be possible to
look up members of I in the class and find them in a base class that is I.
That makes most operations work without special handling.  Also, for many
cases, the template parameter's value is a handle to the constraint class type,
so that operations like assignment work because they become handle assignments
instead of class assignments (where it might not be possible to know whether
the class will have an appropriate ``operator=`` at runtime).  There are a few
special cases with casting, where a generic parameter might or might not be a
value class type, or might or might not effectively be a handle at runtime.
Casts are rejected if they might be invalid because they treat a value type as
a handle, or vice-versa.  Also, casts are rejected if it can be shown at
compile time that the source value type can never satisfy the constraints of
the destination type.

Reference Information
---------------------

To track references to identifiers, ``ref_entry`` is called for each identifier
reference in an expression.  The information provided is used to write
cross-reference information (when requested) and to set the referenced, used,
modified, and address-taken flags in variable and routine entries.  For some
identifiers, the kind of reference is known immediately, so
``record_symbol_reference`` is called to record the information.  For others,
the kind of reference is dependent on the expression context, and therefore it
is not known immediately.  For such cases, an entry called ``a_ref_entry`` is
allocated and placed on a list of such entries for the current expression.
When the reference kind becomes known, the kind in the entry is updated (see
``change_ref_kinds``).  At the end of the expression,
``flush_ref_entries_list`` is called.  It scans the entries on the list, calls
``record_symbol_reference`` to record the (now fully determined) reference, and
then frees the entries.

Diagnostic output
-----------------

Diagnostic output in the expression-processing routines must be handled
specially in order that it be possible to suppress diagnostics when rescanning
an expression during template deduction.  For that reason, what would normally
be simply a call of a diagnostic-output routine must be slightly more
complicated in the expression-processing source files.  For the simpler
diagnostics, there are direct ``expr_`` equivalents:

* | For ``pos_error``, use ``expr_pos_error``.
* | For ``pos_warning``, use ``expr_pos_warning``.
* | For ``pos_diagnostic``, use ``expr_pos_diagnostic``.
* | For ``syntax_error``, use ``expr_syntax_error``.

Other diagnostic calls should be enclosed in a test of
``expr_error_should_be_issued`` or ``expr_diagnostic_should_be_issued``, as
appropriate.

Any access-checking code must be conditioned on
``expr_access_checking_should_be_done``.  It's not enough to simply surround
access-error diagnostic calls with a suppression test; when template deduction
is being done, it must be as if no access checking is even attempted, and the
flow of control must continue as if the error were not present.

Folding of Constant Operations
==============================

There are four main files involved in folding constant operations:

* | ``const_ints.c`` contains the code for low-level integer operations, with
    ``const_ints.h`` containing associated declarations.
* | ``fixed_pt.c`` contains the code for low-level fixed-point operations, with
    ``fixed_pt.h`` containing associated declarations.
* | ``float_pt.c`` contains the code for low-level floating-point operations,
    with ``float_pt.h`` containing associated declarations.
* | ``folding.c`` contains the higher-level code that handles folding of
    constant operations and of type conversions on constants.  ``folding.h``
    contains associated declarations.  Code for folding of ``constexpr`` calls
    and operations is also here.

Low-level Integer Routines
--------------------------

``const_ints.c`` contains the routines that manipulate low-level integers.  Any
integer literal constant in the source program and any operation on constant
integers implied by the source program are ultimately handled by the routines
here.  For the most part, the low-level integer form can be viewed as a
representation for target machine integers on the host machine, although it is
also used for some things (like array dimensions) that are only arguably target
machine constants.

The point of these routines is to collect all the code that operates on target
integers in one place, so that it can be modified if necessary, and so that the
rest of the front end does not need to deal with the subtleties of signed and
unsigned representations, overflow, etc.

There are two different possible representations for integer values (type
``an_integer_value``):

* | If there is some host integer type that is big enough to hold any target
    integer (the usual case), an integer value is simply represented as a host
    integer.  Operations on that representation are done using the host C
    operators, with extra overflow checking.  If the host C compiler supports
    ``long long``, that type can be used as the representation type.
* | Otherwise (for example, in a cross-compiler for a 64-bit target running on
    a 32-bit host), an integer value is represented as a ``struct`` containing
    an array of small integers (of type ``an_int_value_part``).  Operations on
    that representation are simulated in software.

(See configuration constant ``INTEGER_VALUE_REPR_IS_A_HOST_INTEGER``.)

The ``an_integer_value`` representation may be used to represent signed or
unsigned quantities.  It is not self-identifying in that regard, i.e., the
representation does not contain an indication of the signedness of the value.
Therefore, in cases where signedness is significant, (e.g., multiplication),
the caller must pass in a separate argument indicating the signedness to be
used for the integer value.  For values that can be represented in both the
signed and unsigned forms, the representation in the two forms is the same.
That allows small constants to be used as either signed or unsigned quantities
as the need arises.

The ``const_ints.c`` routines deal with integer values as single-sized
entities, the size being large enough to hold any of the target integer types.
The routines check for overflow relative to that representation, but they know
nothing of different integer types and their sizes.

Most routines in ``const_ints.c`` operate on values in the ``an_integer_value``
form.  For convenience, some routines are provided that operate on the IL
``a_constant`` form, but it's a circumspect use of that form: as far as the
routines are concerned, ``a_constant`` is a structure that only contains
``an_integer_value`` (in the variant ``integer_value`` field) and a type (in
the ``type`` field) that can be passed to ``int_constant_is_signed`` to
determine the signedness of the value.  No other fields of the constant need be
defined.

The routines that are used to translate between ``a_host_large_integer`` (some
large built-in host integral type) and the ``an_integer_value`` form are

* | ``set_integer_value``,
* | ``set_unsigned_integer_value``,
* | ``value_of_integer_constant``, and
* | ``unsigned_value_of_integer_constant``.

Routines used to get attributes of integer values are

* | ``get_integer_size_and_alignment`` and
* | ``int_constant_is_signed``.

Utility routines used to determine the magnitude of an integer value, create a
bit mask, and sign-extend a value are

* | ``bits_required_to_represent_integer_constant``,
* | ``make_integer_value_mask``, and
* | ``sign_extend_integer_value``.

Routines used to format integer values in decimal string form are

* | ``str_for_integer_constant``.

Routines used to compare integer values (including some comparisons against the
limits of integers of given kinds) are

* | ``cmp_integer_values``,
* | ``cmp_integer_constants``,
* | ``cmplit_integer_value``,
* | ``cmpulit_integer_value``,
* | ``in_range_for_integer_kind``,
* | ``le_max_value_for_integer_kind``, and
* | ``is_max_value_for_integer_kind``.

On these, the two values being compared may have independent signedness.  The
``lit`` routines do comparison against ``a_host_large_integer`` or
``a_host_large_unsigned`` value, typically a small constant.

Routines used to perform arithmetic and logical operations are

* | ``add_integer_values``,
* | ``subtract_integer_values``,
* | ``multiply_integer_values``,
* | ``divide_integer_values``,
* | ``remainder_integer_values``,
* | ``and_integer_values``,
* | ``or_integer_values``,
* | ``xor_integer_values``,
* | ``shift_left_integer_values``,
* | ``shift_right_integer_values``, and
* | ``complement_integer_values``.

On these, the two operands must have the same signedness (except that there are
versions of the add and subtract routines for the independent-signedness
cases).  The result of the operation overwrites the first operand.

For some kinds of values, it's just too much of a nuisance to use the integer
value routines, so the front end converts such values to
``a_host_large_integer`` or ``a_host_large_unsigned`` (which are typically
signed and unsigned ``long``, or signed and unsigned ``long long``) and
operates on them in that form.  The values handled that way are:

* | Sizes of objects, including arrays (conceptually of type ``size_t`` on the
    target).
* | Offsets relative to objects (conceptually of type ``ptrdiff_t`` on the
    target).
* | Single characters (but not a character constant containing one or more
    characters).
* | Single ``wchar_t`` values.

Note that the target ``size_t`` and ``ptrdiff_t`` are allowed to be bigger than
the host ``a_host_large_integer``; the use of ``a_host_large_integer``
representation just restricts the possible constant values of that type that
the front end can deal with.  The conversions into the host types are done with
overflow checking, so the front end will issue errors for values larger than
those it can accommodate.

Low-level Fixed-point Routines
------------------------------

The routines in ``fixed_pt.c`` manipulate the internal repreentation of
fixed-point constants on the host machine.  As with the integer routines, they
are collected in one place so that they can be changed more easily and so that
the front end at large need know nothing about the fixed-point representation.

They implement very low-level operations on the internal form of fixed-point
constants.  Some of the standard product versions of these routines (e.g., for
conversion of a decimal text form of a fixed-point constant to internal form)
are not always accurate to the last significant bit: They are best viewed as
prototype routines for testing purposes, to be replaced by real routines for
the production version.

The main routines (or macros) in ``fixed_pt.c`` are the following:

* | ``cmp_fixed_point_constants``
* | ``fxp_init_value``
* | ``fxp_value_is_zero``
* | ``fxp_string_to_fixed_point``
* | ``fxp_hex_string_to_fixed_point``
* | ``conv_fixed_point_to_fixed_point``
* | ``conv_integer_to_fixed_point``
* | ``conv_fixed_point_to_integer``
* | ``conv_float_to_fixed_point``
* | ``conv_fixed_point_to_float``
* | ``fxp_to_string``
* | ``fxp_add``
* | ``fxp_subtract``
* | ``fxp_multiply``
* | ``fxp_divide``
* | ``fxp_compare``
* | ``fxp_hash``

Low-level Floating-point Routines
---------------------------------

The routines in ``float_pt.c`` manipulate target floating-point constants on
the host machine.  As with the integer routines, they are collected in one
place so that they can be changed more easily and so that the front end at
large need know nothing about the floating-point representation.

They implement very low-level operations on the internal form of floating-point
constants.  When ``USE_HOST_FP_CONVERSION_ROUTINES`` is TRUE (the default),
binary to decimal conversions are performed using standard UNIX conversion
routines provided by the host.  When ``USE_HOST_FP_CONVERSION_ROUTINES`` is
FALSE, internal routines (in ``floating.c``) are used to perform these
conversions.  Arithmetic operations are performed without range checking;
therefore, they are best viewed as prototype routines for testing purposes, to
be replaced by real routines for the production version.

In cases where the target configuration supports floating-point types that are
not supported by the host compiler (e.g., ``__float128`` and ``__float80`` on
Windows), the `Berkeley SoftFloat library
<http://www.jhauser.us/arithmetic/SoftFloat.html>`__ can be used to perform
arithmetic operations on the host.  This configuration can be selected by
setting the ``USE_SOFTFLOAT`` configuration macro to TRUE.  In such
configurations, the ``USE_HOST_FP_CONVERSION`` routines is automatically set to
FALSE to enable a software-only floating-point configuration.  Note that
such configurations are inherently slower than those that use floating-point
hardware.  See the SoftFloat web site for licensing information.

The main routines in ``float_pt.c`` are the following:

* | ``fp_change_kind``
* | ``fp_string_to_float``
* | ``fp_hex_string_to_float``
* | ``fp_to_string``
* | ``fp_long_to_float``
* | ``fp_unsigned_long_to_float``
* | ``fp_to_host_large_integer``
* | ``fp_to_host_large_unsigned``
* | ``fp_is_zero_constant``
* | ``fp_add``
* | ``fp_subtract``
* | ``fp_multiply``
* | ``fp_divide``
* | ``fp_compare``
* | ``fp_hash``

Constant Folding
----------------

The routines in ``folding.c`` do folding of constant operations and type
conversions on constants.

This folding is done in target machine arithmetic.  For integers, routines in
``const_ints.c`` do the actual operations on the generic integer value, but the
routines here add a layer that understands specific integer types, and does
overflow checking, masking, and sign extension according to the size and
signedness of the integer type.  For fixed-point and floating operations,
target-specific code in ``fixed_pt.c`` and ``float_pt.c`` does the actual
operations.  For pointer operations, the constants are either address constants
(a base symbol plus a byte offset) or integer constants cast to a pointer type
(the most common of these is ``NULL``/0).  The routines ``get_pointer_offset``
and ``set_pointer_offset`` are used to extract and set the offsets in both
those cases.

The folding routines detect errors and warnings and call the error routines to
report them.  When they are called in nonconstant contexts (like executable
statements), errors are downgraded to warnings, and the operation is left
unfolded, to be tried at runtime.

If the folding routines are unable to fold an operation to a constant (because
of some attribute of the operands, not because of an error), they return a flag
``did_not_fold`` set to TRUE, which tells the caller to leave the operation
unfolded.  For example, the pointer difference of pointers to different static
objects is a constant, but one that is unknowable until link time.
``did_not_fold`` would be returned TRUE for that case (and no error or warning
would be indicated).  The folding routines also record the unfolded expression
in constants resulting from the folding.

``unary_operation`` and ``binary_operation`` are the top-level routines for
operations on constants.  They take as input an operator and one or two
constants, and produce a constant (or an error indication) as output.  The
operations are done with all necessary checking for overflows, etc.  The
routines called by ``unary_operation`` to do the actual folding are

* | ``do_inegate``,
* | ``do_fxnegate``,
* | ``do_fnegate``,
* | ``do_xnegate``,
* | ``do_complement``, and
* | ``do_not``,

and those called by ``binary_operation`` are:

* | ``do_iadd``,
* | ``do_isubtract``,
* | ``do_imultiply``,
* | ``do_idivide``,
* | ``do_remainder``,
* | ``do_shiftr``,
* | ``do_shiftl``,
* | ``do_icompare``,
* | ``do_and``,
* | ``do_or``, and
* | ``do_xor``

for integers;

* | ``do_land`` and
* | ``do_lor``

for scalars;

* | ``do_fxadd``,
* | ``do_fxsubtract``,
* | ``do_fxmultiply``,
* | ``do_fxdivide``, and
* | ``do_fxcompare``

for fixed-point;

* | ``do_fadd``,
* | ``do_fsubtract``,
* | ``do_fmultiply``,
* | ``do_fdivide``, and
* | ``do_fcompare``

for floating-point;

* | ``do_xadd``,
* | ``do_xsubtract``,
* | ``do_xmultiply``,
* | ``do_xdivide``,
* | ``do_jmultiply``,
* | ``do_jdivide``, and
* | ``do_xcompare``

for complex and imaginary;

* | ``do_padd``,
* | ``do_pdiff``, and
* | ``do_pcompare``

for pointers; and

* | ``do_pmcompare``

for pointers to members.

Actual shifting is performed by ``do_shift``, which calls ``check_shift_count``
for error checking.  The shifting is implemented with the integer size and
sign-extension appropriate for the target.

When pointer addition or subtraction is folded, the resulting constant, if an
address constant, is checked to see if the offset lies within the base object.
Offsets beyond the end of the object and negative offsets are flagged with a
warning (most often, this is due to a constant subscript out of bounds).  This
is checked by ``valid_address_constant``.

In ordinary constant folding, ``valid_address_constant`` will allow the
position just past the end of an object without a warning, since it does not
know whether the address of that position is required (which is allowed by ANSI
C) or the value (which is an error).  The routine ``using_lvalue`` calls
``valid_address_constant`` again to check for the error once it is known that
it is the value that is being accessed.

``type_change_constant_full`` changes the type of a constant to something else.
It's usually called through the simpler interface ``type_change_constant``.  It
has subroutines

* | ``conv_integer_to_integer``,
* | ``conv_integer_to_float``,
* | ``conv_float_to_integer``,
* | ``conv_float_to_float``,
* | ``conv_pointer_to_whatever``,
* | ``conv_integer_to_pointer``,
* | ``conv_ptr_to_member_to_ptr_to_member``, and
* | ``conv_integer_to_ptr_to_member``.

with the obvious functions.  Warnings about truncation and hidden changes of
sign are issued (the latter are suppressed if the type change was the result of
an explicit cast or if the operand was a non-decimal constant).

Casts of pointers to classes to pointers to base or derived classes are handled
by

* | ``fold_base_class_cast`` and
* | ``fold_derived_class_cast``.

The similar cases for pointers to members are handled by

* | ``fold_pm_base_class_cast`` and
* | ``fold_pm_derived_class_cast``.

``fold_field_selection`` is called to fold a field selection relative to a
constant address.  Because of the unusual second operand (a field rather than a
constant), ``binary_operation`` could not be used for this case.  If the field
is a bit field, ``fold_field_selection`` returns a ``did_not_fold`` flag.

``constant_glvalue_address`` and its companion routine
``constant_prvalue_pointer`` examine expression trees to determine if they
represent constant addresses, computing and returning the appropriate constant
value on the fly.

.. _constexpr-and-expr-folding:

Folding and ``constexpr``
-------------------------

C++11 and C++14 permit calls to ``constexpr`` functions and constructors in
constant-expressions.  To implement this, the front end includes an IL
interpreter implemented in the source file ``interpret.c``.  The start of that
file contains extensive comments describing the overall structure of the
interpreter.  The global variable ``relaxed_constexpr_enabled`` is TRUE
when the C++14 version of the ``constexpr`` feature is enabled.  This enables
the interpretation of expressions, like assignments, with side-effects (valid
in C++14, but not C++11).  It also removes C++11 structural constraints for
``constexpr`` functions and constructors (e.g., it allows multiple statements
in definitions).

The global variable ``constexpr_exceptions_enabled`` is TRUE when exceptions
can be thrown and caught during constant evaluation (a C++26 feature; it can
also be controlled with ``--set_flag`` and ``--clear_flag`` options naming
``constexpr_exceptions``, and it is FALSE whenever ``exceptions_enabled`` is
FALSE).  The interpreter then evaluates ``throw`` expressions and ``try``
blocks: A thrown exception unwinds the interpreted evaluation, destroying
objects along the way, until a matching handler is found.  An exception that
is not caught within the evaluation, or that causes ``std::terminate`` to be
called, makes the interpretation fail.  The section "Control Transfers and
Exceptions" in the comments at the start of ``interpret.c`` describes the
implementation.

To avoid burdening the IL or the rest of the front end, the interpreter does
not record state information in the IL or other front end structures.  Instead,
it maintains its own data structures linked to the IL through efficient hash
tables.  It does, however, use front end types to represent integral and
floating-point values.  Other kinds of values (e.g., addresses) are represented
using interpreter-specific structures.

The interpreter can fold calls to ``constexpr`` functions and constructors, but
it can also more generally fold expressions and dynamic initializations.

The entry points into the interpreter are ``interpret_expr,``\
``interpret_dynamic_init,``\ ``interpret_constexpr_call,`` and
``interpret_constexpr_ctor``.  These functions return a boolean value
indicating whether interpretation succeeded (TRUE) or failed (FALSE).
Interpretation can fail because it encountered an operation not permitted
during ``constexpr`` evaluation (e.g., a virtual function call), including
operations with undefined behavior (e.g., attempting to fetch a value from
uninitialized storage).  It can also fail when the cost of interpretation
becomes too high.  The parameters limiting that cost can be set through the
command-line options ``--max_depth_constexpr_call`` and
``--max_cost_constexpr_call``.  If interpretation fails, the interpreter also
returns a diagnostic description indicating the reason for the failure (that
description can be emitted as part of the diagnostic if a constant was
required).

.. [#f1] Note that the C standard does not actually have an rvalue concept,
         though it has lvalues.  This is a difference from K&R.  However, the
         distinction between an rvalue and a non-lvalue is almost nonexistent.
         It has to do with some strange cases (like ``sizeof``) that allow
         unevaluated non-lvalues.  For simplicity, the term "rvalue" is used,
         but what is actually implemented by the front end is the ANSI C
         non-lvalue.
.. [#f2] Following the implementation of C++11 value categories in version
         4.8, the front end has tried to use the C++11 terms in general
         internally.  That caused a lot of renaming of routines and fields; for
         example, ``conv_lvalue_to_rvalue`` became ``conv_glvalue_to_prvalue``.
         Ugly until you get used to it, but very helpful in reminding you to
         consider xvalues when you write code that deals with expressions.
.. [#f3] Meaning, more or less, when the glvalue-to-prvalue conversion is done
         on the operand.  Constant-valued variables are replaced by their
         values at that point, and member access and subscripting operations,
         and pointer dereferences, produce constant values only when the
         glvalue addressing expression is converted to a prvalue.
.. [#f4] This is used, for example, for the initializer expression on a
         const-typed variable; the initializer is not required to be a constant
         expression, but if it is, the variable is usable as a constant.  So
         the expression is scanned as a normal (non-constant) expression, and
         if it produces a constant value and does not violate the rules for a
         well-formed constant expression, the variable is a constant.
.. [#f5] Actually, it is also called -- during prototype instantiations -- when
         at least one operand has a template-dependent type.  In that case, it
         generates a generic expression and returns without doing overload
         resolution.
