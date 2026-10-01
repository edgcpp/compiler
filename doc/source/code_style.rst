==================
Coding Style Guide
==================


General Whitespace Rules
========================

* Maximum line length is 79 characters.
* Indentation is by 2 spaces.
* Editors should be set to use an 8 space tab width.

General Line Wrapping
=====================

General Condition Rules
-----------------------

Conditions that cause the line length to exceed 79 characters are wrapped via a
line break following an operator.  The continuation is left aligned with the
opening paren:

.. code:: cpp

   if (/* something very long */ &&
       /* something else */) {
     /* etc... */
   }  /* if */

   if ((a || b) &&
       (/* something very long */ ||
        /* something else */)) {
     /* etc... */
   }  /* if */

   if ((a || b) && (/* something very long */ ||
                    /* something else */)) {
     /* etc... */
   }  /* if */

General Statement Rules
-----------------------

Expressions that cause the line length to exceed 79 characters are wrapped via
a line break following an operator.  The continuation is right-aligned:

.. code:: cpp

   int x = /* something very long */ &&
                           /* something else (right-aligned to column 79) */;

General Line Breaks
===================

Line breaks are prioritized as follows:

* 1 line for a conditional preprocessor directives (e.g., #if, #ifdef, etc.)
  unless adding whitespace would break a visual "grouping" (e.g., conditional
  inclusion of a header in a list of headers, conditionally enabling a single
  statement in a function, etc.).
* 1 line for a namespace declaration and its associated closing comments.
* 2 lines for a function definition.
* 1 line for a function, variable, or type declaration.

Naming Convention
=================

The EDG naming convention can be broadly described as "snake case" (with an
exception for templates).  In snake case, names are composed of lower case
letters and underscores.

For types, the naming convention additionally adds a "``a_``/``an_``"
prefixed to the typename.  For instance, what (in many projects) would be
spelled ``struct thing`` is called ``struct a_thing``.

Writing a Variable
==================

Variables appearing in "global" (these technically shouldn't exist as
there's a conditional "``edg``" namespace around all file contents) or
namespace scope are indented using tabs in older files which make use of
tabs and the equivalent
(8) spaces in newer files.  The type is not indented, the name is indented by
2 tabs, and a comment should be added at 3 tabs:

.. code:: cpp

   a_variable_type_name
           my_variable;
                   /* A descriptive comment. */

If a variable type or name ends before the next tab stop, the lines should be
collapsed:

.. code:: cpp

   a_variable_type_name
           x;      /* A descriptive comment. */
   int     my_variable;
                   /* A descriptive comment. */
   int     x;      /* A descriptive comment. */

.. _writing-a-function:

Writing a Function
==================

Parameter Alignment
-------------------

Functions declarations that fit within a single line should do so:

.. code:: cpp

   void foo(int a, int b, int c);

If a function declaration does not fit on a single line, it should be broken up
with the parameters broken up into two left-aligned columns, one column of
parameter types, and one column of parameter names:

.. code:: cpp

   void foo(a_variable_type        thing,
            an_other_variable_type other_thing,
            a_thingy               thingy);

Wrapping Function Declarations
------------------------------

Particularly for function definitions with lots of qualifiers and long names
(member functions are particularly guilty), simply wrapping the parameters can
be insufficient.  In these cases, there are two accepted techniques for keeping
the function within the required line length.

If the function has qualifiers like ``static`` and ``inline`` these can be
moved to the preceding line along with the return type:

.. code:: cpp

   static inline void
   very_long_long_long_long_foo(a_variable_type        thing,
                                a_nother_variable_type other_thing,
                                a_thingy               thingy);

If this is still insufficient, the parameters can be moved to a new line and
right-aligned:

.. code:: cpp

   void very_long_long_long_long_long_foo(
                                       a_variable_type        thing,
                                       a_nother_variable_type other_thing,
                                       a_thingy               thingy);

Documentation
-------------

In a function definition, a comment should be added after the closing paren but
before the opening curly brace.  This comment should describe all of the
parameters, the return value, and any actions it takes, and the closing
``}`` of the function should be followed by a comment giving the name
of the function, separated from the ``}`` by two spaces:

.. code:: cpp

   a_boolean foo(int num_l_parens, int num_r_parens)
   /*
   Given the number of left parens (num_l_parens) and the number of right
   parens (num_r_parens), display the values and return TRUE if the
   numbers are equivalent; otherwise, return FALSE.
   */
   {
     printf("lparens=%d, rparens= %d\n", num_l_parens, num_r_parens);
     return num_l_parens == num_r_parens;
   }  /* foo */

This also applies to void functions that use output parameters:

.. code:: cpp

   void foo(a_boolean *result, int num_l_parens, int num_r_parens)
   /*
   Given the number of left parens (num_l_parens) and the number of right
   parens (num_r_parens), assign *result to TRUE if the numbers are equivalent;
   otherwise, assign *result to FALSE.
   */
   {
     *result = (num_l_parens == num_r_parens);
   }  /* foo */

Return Value
------------

If a function requires a variable to hold the result, it should be the first
variable in the function, and it should be named "``result``":

.. code:: cpp

   int foo()
   /*
   A descriptive comment.
   */
   {
     int result;

     /* etc... */
     return result;
   }  /* foo */

Variable Grouping
-----------------

Variables should be grouped into blocks with their types and names aligned.
The first block of variables should have a trailing newline, and any following
blocks should use a preceding newline:

.. code:: cpp

   int foo()
   /*
   A descriptive comment.
   */
   {
     int       result;
     a_boolean temp_state;

     /* etc... */

     a_state_catcher secondary_state;
     a_boolean       state_catcher_used = FALSE;
     /* etc... */
     return result;
   }  /* foo */

Writing a Lambda
================

Lambdas are a case that's a mix of variable and function rules.  Lambda
parameters -- similar to function parameters -- should be broken up into two
left-aligned columns, one column of parameter types, and one column of
parameter names:

.. code:: cpp

   auto x = [](int num_l_parens,
               int num_r_parens) -> a_boolean {
     return num_l_parens == num_r_parens;
   };

Unlike functions, lambdas do not require a documentation comment, or a closing
comment.  For this reason, lambdas should be reserved for fairly trivial
operations.

If a lambda appears in a block of other variables, the lambda should follow the
name alignment of the variables in the same block, e.g.,

.. code:: cpp

  a_long_type_name y = /* etc... */;
  auto             x = [](int num_l_parens,
                          int num_r_parens) -> a_boolean {
    return num_l_parens == num_r_parens;
  };

Writing a Class
===============

Classes should be preceded by a descriptive comment, and terminated by a
closing comment giving the class name and separated from the ``;`` by two
spaces:

.. code:: cpp

   /*
   A descriptive comment about class foo.
   */
   struct foo {
     /* etc... */
   };  /* foo */

Data Members
------------

Data members should be written at the end of the class.  In terms of
indentation, data members are indented similarly to variables in global &
namespace scope, with the exception that the type is always indented by two
spaces:

.. code:: cpp

   /*
   A descriptive comment about class foo.
   */
   struct foo {
     /* etc... */
   private:
     a_variable_type_name
                   my_var_a;
                           /* A descriptive comment. */
     a_variable_type_name
                   b;      /* A descriptive comment. */
     int           my_var_c;
                           /* A descriptive comment. */
     int           d;      /* A descriptive comment. */
   };  /* foo */

Member Functions
----------------

Member functions (including special member functions like constructors and
destructors) that do not exceed 1 line can be written inline:

.. code:: cpp

   /*
   A descriptive comment about class foo.
   */
   struct foo {
     void bar()
       { this->x += 1; }
   private:
     int           x;      /* A descriptive comment. */
   };  /* foo */

If the member function is empty it should be spelled "``{}``" (no space):

.. code:: cpp

   /*
   A descriptive comment about class foo.
   */
   struct foo {
     void foo()
       {}
   };  /* foo */

Member functions that exceed 1 line should be written out-of-line:

.. code:: cpp

   /*
   A descriptive comment about class foo.
   */
   struct foo {
     void complex_bar();
   private:
     int           x;      /* A descriptive comment. */
   };  /* foo */


   void foo::complex_bar()
   /*
   A descriptive comment about complex_bar.
   */
   {
     /* etc... */
   };  /* foo::complex_bar */

Additionally see :ref:`writing-a-function` as those rules also apply to member
functions.

Member Initializer
------------------

If initializing using a passed value, the data members and parameters
should have names that differ (e.g., for a data member ``x``, ``x_val`` can
be an appropriate parameter name).

Additionally, the introducing colon and all member initializers should be
indented two spaces, e.g.,

.. code:: cpp

   /*
   A descriptive comment about class foo.
   */
   struct foo {
     void foo()
       : x(1), y(2), z(3)
       {}
     inline void foo(int x_val, int y_val, int z_val);
     int           x;      /* A descriptive comment. */
     int           y;      /* A descriptive comment. */
     int           z;      /* A descriptive comment. */
   };  /* foo */


   void foo::foo(int x_val, int y_val, int z_val)
   /*
   A descriptive comment about foo::foo.
   */
     : x(x_val + 100), y(y_val + 1000),
       z(z_val + 10000)
   {
     /* etc... */
   };  /* foo::foo */

Statements
==========

``for`` Statements
------------------

C-style
^^^^^^^

"C-style" ``for`` loops should be formatted with their contents indented by 2
spaces, and a closing comment (with two preceding spaces):

.. code:: cpp

   for (int x = 0; x < 10; ++x) {
     /* etc... */
   }  /* for */

If the condition of the ``for`` loop causes the line length to exceed 79
characters, it should be broken up at the semicolons:

.. code:: cpp

   for (int long_variable = 0;
        long_variable < another_long_variable;
        ++long_variable) {
     /* etc... */
   }  /* for */

Range-based
^^^^^^^^^^^

Range-based ``for`` loops should be formatted as "C-style" ``for`` loops
are, with the exception that, if the condition cause the line length to
exceed 79 characters, it should be broken at the colon and right aligned:

.. code:: cpp

   for (a_long_type &&x :
            some_container) {
     /* etc... */
   }  /* for */

``if`` Statements
-----------------

``if`` statements should be formatted with their contents indented by 2
spaces. ``else`` and ``else if`` clauses should be preceded and followed by
braces, aligned with the ``if``, and the closing brace aligned with the
``if`` and followed by an ``if`` comment (with two preceding spaces):

.. code:: cpp

   if (argument) {
     /* something */
   } else if (other_argument) {
     /* something else */
   } else {
     /* default action */
   }  /* if */

``switch`` Statements
---------------------

``switch`` statements should be formatted with their case labels indented
by 2 spaces, the contents further indented by two spaces, and the closing
brace annotated with a comment (with two preceding spaces). The opening
brace of a scoping block within a case can be immediately followed on the
same line by the first statement of the block, and its closing brace should
have no comment.  If one case intentionally omits the usual closing
``break`` statement and flows into the next, the intent should be
documented by use of the ``FALLTHROUGH`` macro. When the cases of a
``switch`` statement are intended to cover all the enumerators of an
enumeration, the ``default_is_unexpected`` macro should be used to trigger
compiler diagnostics if an enumerator is added and not explicitly handled
by the ``switch``:

.. code:: cpp

  switch (argument) {
    case a:
      { int y = 10;

        x += y;
      }
      FALLTHROUGH
    case b:
      x += 1;
      break;
    case c:
      /* no-op */
      break;
    default_is_unexpected();
  }  /* switch */

``while`` Statements
--------------------

Pre-condition
^^^^^^^^^^^^^

"Pre-condition" ``while`` statements should be formatted with their contents
indented by 2 spaces, and a closing comment (with two preceding spaces):

.. code:: cpp

   while (argument) {
     /* etc... */
   }  /* while */

Post-condition
^^^^^^^^^^^^^^

"Post-condition" (i.e., ``do``) while statements should similarly be
formatted with their contents indented by 2 spaces.  However, unlike
"pre-condition" ``while`` statements, no closing comment is required:

.. code:: cpp

   do {
     /* etc... */
   } while (argument);

Preprocessor Directives
=======================

``#if`` and ``#endif``
----------------------

``#if`` should have its closing #endif commented with the condition used
(preceded by a single space):

.. code:: cpp

   #if MACRO_IS_ENABLED
   /* etc... */
   #endif /* MACRO_IS_ENABLED */

If the condition is complicated, the closing comment can be abbreviated:

.. code:: cpp

   #if MACRO_IS_ENABLED && (OTHER_MACRO_IS_ENABLED || \
                            OTHER_OTHER_MACRO_IS_ENABLED)
   /* etc... */
   #endif /* MACRO_IS_ENABLED && (OTHER_MACRO...) */

``#else``
---------

If an ``#if`` makes use of an ``#else`` a comment should express the negated
condition (preceded by a single space):

.. code:: cpp

   #if MACRO_IS_ENABLED
   /* etc... */
   #else /* !MACRO_IS_ENABLED */
   /* etc... */
   #endif /* MACRO_IS_ENABLED */

``#elif``
---------

If an ``#if`` makes use of an ``#elif``, no change to comments is performed:

.. code:: cpp

   #if MACRO_IS_ENABLED
   /* etc... */
   #elif OTHER_MACRO_IS_ENABLED
   /* etc... */
   #endif /* MACRO_IS_ENABLED */

Templates
=========

Templates follow the rules of the respective thing being templated, with
the exception that the name for templates declaring a type should not
follow the normal naming convention.  For class and alias templates, no
``a_`` or ``an_`` qualifier is used, instead an uppercase letter starts the
name (e.g., ``Dyn_array``).

Template Parameters
-------------------

Template parameters, like template declarations, have their own naming
convention.  They should be named ``a_``/``an_`` followed by a capital
letter, and then a descriptive name, e.g.,

.. code:: cpp

  template<typename an_Elem>
  struct My_template;

If multiple template parameters need to be denoted as the "1st, 2nd, 3rd,
etc."  version of a parameter, capital letters can be used to denote this,
e.g.,

.. code:: cpp

   template<typename a_Type_A, typename a_Type_B>
   struct Thingy_helper;

Special Cases
=============

Pointer & Reference Types
-------------------------

The rule for pointer and reference types is that the pointer or ref qualifier
accompanies the name being typed (for purposes of a function, consider the
return type the "type" of the function) e.g.,

.. code:: cpp

   void foo(int *result, const thingy &a)
   /*
   A descriptive comment.
   */
   {
     int &c = a.value;

     *result = c;
   }  /* foo */


   a_thingy *alloc_thingy();

Empty Loop Bodies
-----------------

Empty compound statements (e.g.,  function bodies and loop bodies) should be
spelled "``{}``" (no space) and follow the looping statement on the same line,
e.g.,

.. code:: cpp

   for (x = 0; x < y; x *= 2) {}

   while (try_op(x)) {}

Annotating Function Arguments
-----------------------------

When a function is called with an argument of a fixed value (e.g., a
``TRUE`` or ``FALSE`` value for a "flag" argument) the argument should be
annotated with a comment naming the associated parameter, e.g.,

.. code:: cpp

   int chars = count_characters(str, /*include_whitespace=*/FALSE);

When ``NULL`` is passed, it should either follow the same convention or
be cast to the parameter's type, e.g.,

.. code:: cpp

   some_func(one_thing, /*associated_thing=*/NULL);
   other_func(something_else, (a_foo*)NULL);

Wrapping Function Calls
-----------------------

Function calls that cause the line length to exceed 79 characters can be
handled one of two ways.

If the function call is initializing a variable, and would otherwise fit on the
line, it can be moved down a line and right aligned:

.. code:: cpp

   int x =
        my_very_long_function(a, b, c, d, e, f, g, h);

Otherwise, the function call should be wrapped on the arguments, aligning
continuation lines with the "``(``" of the function call; if there is a
single argument that cannot fit between the "``(``" column and the right
margin, the call should be broken after the "``(``" and all continuation lines
aligned with the position resulting from right-justifying the longest
argument.  The exact style for wrapping the arguments is not specified, but
in general developers should, in the interest of compact code, flow as many
arguments together as will fit on each line:

.. code:: cpp

   int x = my_very_long_function(a, b, c, d, e, f,
                                 g, h);

If arguments have some logical relation, e.g., for an area function, these
arguments can be grouped to emphasize the relation instead of flowing for
maximal compactness:

.. code:: cpp

   compute_area_of_points(x1, y1, z1,
                          x2, y2, z2);

In cases where the arguments are annotated with embedded comments, it may be
desirable to distribute them across multiple lines for clarity and neatness,
e.g.,

.. code:: cpp

   f(a, b, c, d, /*is_foo_bar=*/FALSE,
     /*needs_more_squirrels=*/VERY_FALSE);

instead of:

.. code:: cpp

   f(a, b, c, d, /*is_foo_bar=*/FALSE, /*needs_more_squirrels=*/VERY_FALSE);
