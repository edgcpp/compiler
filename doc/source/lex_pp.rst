==================================
Lexical Analysis and Preprocessing
==================================

Source Line Data Structure
==========================

Before describing the routines involved in lexical analysis and preprocessing,
it is necessary to discuss the data structure that represents the current
source line.  This data structure is used throughout the lexical routines, and
its form is dictated by the types of processing that must be done in the
lexical and macro routines.  The routines and data structures described in this
section are defined in ``lexical.c`` and ``lexical.h``.

The Current Source Line
-----------------------

A logical source line, in the ANSI C standard (2.1.2.2), is what results after
trigraphs have been replaced by the corresponding characters (e.g., ``??=`` by
``#``) and line splices have been done (i.e., each line ending with a backslash
is spliced with the following line).  The routine ``read_logical_source_line``
does this processing, and the global array ``curr_source_line`` contains the
result.  That is, in ``curr_source_line`` trigraphs have already been replaced,
and several physical source lines may have been spliced into one logical source
line.  If any such transformations were made to produce the current source
line, the global variable ``orig_line_modif_list`` points at a list of entries
that describe all the transformations made, in a form such that

* | the original source lines can be reconstructed from ``curr_source_line``,
    if that is necessary (e.g., for error-reporting purposes); and
* | from a location within ``curr_source_line``, one can determine the file
    name, line number, and column from which that text came, for
    error-reporting purposes.

.. _lex-src-line-modifications:

Modifications to the Current Source Line
----------------------------------------

When changes to the current logical source line are made because of
preprocessing, they are handled in a different way: ``curr_source_line`` is not
changed (much); instead, the global variable ``source_line_modif_list`` points
to a list of entries that indicate changes that have been made logically to
``curr_source_line``.  Each of these changes is a deletion of some number of
characters at a given location, and an insertion of some number of characters
(possibly none) at that location.  These entries represent changes due to
deletion of comments and replacement of macro invocations by the expansions of
those macros.  (Comments need not be deleted, even logically, if preprocessing
output is not being produced.  In ``pcc`` mode, comments are deleted entirely,
but in other modes they are replaced by one blank.)

The two sets of changes are handled differently, and one might ask why the
trigraphs and line splices are actually rendered in ``curr_source_line`` (with
instructions on undoing them), while the macro changes are not rendered (and
the instructions are for doing them rather than undoing them).  The reason is
that the first set of changes is applicable at the character or intra-token
level, and the second set only applies at the inter-token level.  It is a basic
goal of the lexical routines that the current position must be representable as
a single pointer, and that one be able to move forward efficiently in the input
by just incrementing that pointer.  Trigraphs and line splices can occur within
tokens, and therefore if they were not rendered in ``curr_source_line`` one
would have to check for them in every case where one wants to move forward in
the input.  The other modifications, on the other hand, can only occur between
tokens, and therefore they need not be expanded in the source line.  They occur
between tokens, and special marker characters in the input stream are used to
indicate their presence.  The marker characters cause the termination of
scanning of the previous token, followed (on the next call of ``get_token``) by
interpretation of the marker character to decide where the input goes next.
This makes macro expansion efficient (the source line need not actually be
subjected to insertions and deletions) while preserving the ability to advance
through the input using just one pointer.  It also preserves information that
allows one to construct either the original source line or the macro-expanded
line, and to know the origin of any particular part of the resultant line.

The modification entries can apply to the inserted text of other modifications,
so one can have modifications of modifications.  Wherever a modification is
made, the original character (in ``curr_source_line`` or inserted text) is
replaced by a marker character (``ATTENTION_MARKER``) so that if that spot is
reached by sequential scanning from preceding text, one is cued to look at the
modification list to find the text that follows.

In extreme cases the data structure can get quite complex, with modifications
on modifications.  In cases involving real-world macros, however, it remains
fairly simple.

``ATTENTION_MARKER`` is a one-character lexical escape sequence.  It has to be,
because a macro invocation can be as short as a single character, and the
``ATTENTION_MARKER`` overwrites the beginning of the macro invocation.  There
are also two-character lexical escape sequences, which begin with the
``LE_ESCAPE`` character (which is zero).  The second character of the escape
sequence is one of the following:

* | ``LE_END_OF_LINE``, which marks the end of the primary source line.
* | ``LE_NEWLINE``, which represents the newline character at the end of the
    primary source line.
* | ``LE_END_OF_INSERTION``, which marks the end of the text inserted by a
    source line modification.
* | ``LE_END_OF_TOKEN``, which separates tokens in macro text (see below).
* | ``LE_INERT_MACRO``, which indicates a macro name that should not be
    expanded (see below).
* | ``LE_NULL``, which indicates a null (zero) character in the source line\
    ``.``

To ensure that the ``ATTENTION_MARKER`` and ``LE_ESCAPE`` characters are always
lexical escapes, rather than characters accidentally introduced into the
lexical data structure because of special characters in the input file, the
front end does two rewrites on input:

* | The newline character at the ends of input lines is transformed into a
    lexical escape sequence.  This frees up the newline character to be used as
    ``ATTENTION_MARKER``.
* | A null (zero) character in an input line is flagged with an error and
    replaced by a space.  In modes that allow null characters in the source
    (e.g., gcc mode), the character is replaced by an ``LE_NULL`` escape
    sequence (and an original line modification is created, so that column
    numbers can be determined correctly).

The characters newline and null were selected for use as these reserved
characters because they are already special in the C language and in character
sets in general.  No other pair of characters can be guaranteed not to occur in
source files, especially when international character encodings such as SJIS
are considered.

Note that because ``LE_ESCAPE`` is a zero character, the source line and
inserted text cannot be considered to be simple null-terminated strings.  Also
note that the ``ATTENTION_MARKER`` and ``LE_ESCAPE`` characters will naturally
terminate the accumulation of the characters of a token.

The modification entries of both kinds are dynamically allocated, used, and
then freed when the next logical source line is about to be read.  For each
kind, there is an available list (freed entries that are available for reuse),
an allocation routine, and a free routine.  See

* | ``add_orig_line_modif``,
* | ``free_orig_line_modif``,
* | ``add_source_line_modif``,
* | ``free_source_line_modif``, and
* | ``rem_source_line_modif``.

The majority (75%?) of source lines have no modifications of any kind (not
counting comments), so the code in the lexical routines is optimized to deal
efficiently with that case.

Routines ``assoc_source_line_modif`` and ``nested_source_line_modif``, along
with the associated macros ``go_into_insertion`` and ``leave_insertion``,
handle the low-level tasks of determining where to go next in the input stream
based on the source modifications.

The current position in the input stream in given by ``curr_char_loc``.
Ordinarily, it points to somewhere within ``curr_source_line``, but when macros
are expanded it can point to somewhere in ``macro_buffer`` or to a
dynamically-allocated argument.

In the text of macro arguments and the replacement text for macros, the
individual characters have already been tokenized once.  To ensure that the
tokenization is done the same way if and when it is done again, the text of
each token in source line modifications is followed by an ``LE_END_OF_TOKEN``
lexical escape sequence.  That escape sequence terminates the accumulation of
characters to make up a token, and is then ignored as part of the white-space
skip at the start of the next call of ``get_token``.  End-of-token sequences
are usually removed by ``gen_pp_output_for_curr_line``, but an individual
end-of-token will be output as a blank if there is the possibility that two
adjacent tokens might be misparsed without one.  See the discussion in
``macro.c`` for further information.

In some obscure cases involving thwarted macro expansion, it is necessary to do
a true insertion into the source line, rather than the usual replacement of
some characters by some others.  Fortunately, this only needs to be done at the
beginning of the line, so a simple mechanism suffices: a source line
modification with a NULL location is taken to be an insertion before the first
character of the line.  Only one such insertion is allowed per line, and if one
exists, ``line_start_source_line_modif`` points to it.

``curr_source_line`` and ``macro_buffer`` are actually dynamically-allocated
arrays, and can be expanded (by reallocation) as necessary.  Therefore, there
is no limit to the length of source lines or of macros except that imposed by
available memory.

Lexical Analysis
================

"Lexical analysis" includes the reading of source lines and the breaking of
them into lexical tokens.  It is also closely tied to preprocessor directives
and to macro processing, since they are invoked at the lexical level, and are
defined in terms of tokens.  The source code for lexical processing is in
``lexical.c``, and associated declarations are in ``lexical.h``.

Getting Tokens
--------------

The primary interface to the lexical routines is the routine ``get_token``,
which fetches and returns the next token of input.  When compiling C, these
tokens are the input for syntax analysis.  When preprocessing only, the tokens
are fetched (by a loop in ``cpp_driver`` in ``preproc.c``), and then discarded;
calling ``get_token`` repeatedly forces the reading of input lines and the
necessary preprocessing on each line, and each modified line is then written
out just before the next line is read.  ``do_preprocessing_only`` and
``generate_pp_output`` will be TRUE in that case.  ``get_token`` is used in
both cases; in fact, it is used for all fetching of tokens in all contexts.

When ``get_token`` is called, it scans the next token of input and returns its
kind.  The kind of token is also saved in ``curr_token``.  See the enumeration
``a_token_kind``, in ``lexical.h``, for the list of token kinds (e.g.,
``tok_identifier``, ``tok_lparen``).

Much of the rest of the processing is dependent on several global switches, as
described in the following paragraphs.  Most of these switches are defined in
``preproc.h``.

If ``fetch_pp_tokens`` is TRUE when ``get_token`` is called, a preprocessing
token (pp-token) is fetched instead of a regular token.  Constants are not
converted (and numeric ones are returned as pp-numbers); adjacent string
literals are not concatenated; keywords are not recognized; and errors are not
issued for malformed tokens.  Also, ``start_of_curr_token`` and
``end_of_curr_token`` are set to point to the beginning and end of the current
token, and ``len_of_curr_token`` is set to its length.

If ``expand_macros`` is TRUE, preprocessing macros are expanded as they are
scanned.  The caller sees only the tokens after expansion.  If a macro name is
preceded by an ``LE_INERT_MACRO`` escape sequence, the macro is not expanded
and the identifier is returned to the caller with ``curr_token_is_inert_macro``
set to TRUE.

When ``fetch_pp_tokens`` is FALSE, ``do_string_literal_concatenation`` controls
whether adjacent string literals are concatenated.

If ``in_preprocessing_directive`` is TRUE, the definition of white space is
changed to that for within preprocessing directives; i.e., keywords are not
recognized, and newline, ``#``, and ``##`` are recognized and returned as
tokens.  However, if ``caching_pragma_tokens`` is TRUE, a pragma is being
recoded as a token cache, and this requires disabling some of the processing
that is normally done when ``in_preprocessing_directive`` is TRUE --
identifiers will be looked up and ``#`` and ``##`` are not returned as tokens.
And if ``recognize_keywords_in_pragma`` is also set, keywords are looked up and
returned as keyword tokens rather than as identifier tokens.  (This is required
for dealing with ``#pragma`` directives involving C or C++ constructs that must
be scanned by the compiler's normal scanning routines; for instance, it is used
for processing the pragmas that control template instantiation.)

If ``in_pp_if_expression`` is TRUE (indicating that we are inside a
preprocessing ``#if`` expression), integer constants will get an implicit ``L``
suffix, and undefined identifiers will be returned as the integer constant
``0L``.

If ``exp_header_name`` is TRUE, then ``"..."`` will be scanned as a header name
for an ``#include`` (``tok_header_name``).  Other tokens will be processed
normally.  ``exp_system_header_name`` is similar, for header names of the form
``<...>``.

If ``exp_digit_sequence`` is TRUE (indicating a string of decimal digits) then
the digits will be scanned as a ``tok_digit_sequence`` (used in the ``#line``
directive).  Other tokens will be processed normally.

For tokens that are literal constants, the struct ``const_for_curr_token`` is
set to the value of the constant (if ``fetch_pp_tokens`` is FALSE).  This
global variable is also set for the literal portion of user-defined literals
(when user_defined_literals_enabled is TRUE), along with
ud_lit_op_sym_for_cur_token, which designates the symbol for the literal
operator or literal operator template chosen to produce the value of the
user-defined literal.  When ud_lit_op_sym_for_curr_token designates a raw
literal operator or a literal operator template, const_for_curr_token is a
ck_string constant whose value is the actual spelling of the literal part of
the toke; otherwise, it is the value of that literal part.

For tokens that are identifiers, the corresponding symbol header is looked up
in the symbol table and ``locator_for_curr_id`` is set accordingly.  For
user-defined literal tokens, ``locator_for_curr_id`` is set to designate the
*literal-operator-id* of the literal operator or literal operator template
denoted by the literal's *ud-suffix*.  It can be used later to refer to the
current definition of the identifier or operator (if there is one) or to enter
a new definition for the identifier or operator.

The routine ``get_token`` is basically one large ``switch`` statement that fans
out based on the current character of input.  The various cases are:

* | For operators and similar special-character tokens, ``get_token`` looks at
    the next character or two and selects the appropriate token.
* | For the lexical escape sequences (the ATTENTION_MARKER character or the
    two-character LE_ESCAPE sequences), ``skip_white_space`` is called.  These
    characters are not all actually white space, but it simplifies processing
    to let that routine handle them.  All reads of new source lines are
    requested by ``skip_white_space``.
* | For numbers, ``scan_number`` is called (it handles decimal, octal, and
    hexadecimal integer literals, as well as decimal and hexadecimal
    fixed-point and float literals).  ``scan_number`` will determine the kind
    of number, accumulate its characters, and, if appropriate, call the proper
    conversion routine in ``literals.c`` to convert the literal constant string
    to internal form in ``const_for_curr_token``.  It also handles recognition
    of numeric user-defined literals and, when one is recognized, setting
    ud_lit_op_sym_for_curr_token.
* | For identifiers, the characters are accumulated and ``find_symbol`` is
    called.  This yields a list of symbols with the given name.  The list is
    then searched for a definition that is a macro or keyword.  If the
    identifier is a macro and if macro expansion is required,
    ``macro_invocation`` (in ``macro.c``) is called.  Note that because this
    look-up finds both macros and other uses of identifiers, a symbol only has
    to be looked up once; it does not have to be looked up once to see whether
    or not it is a macro, and then again later to see if it is something else.
* | For quoted tokens (character constants and string literals, and the wide
    versions thereof), an appropriate scanning routine is called, which in turn
    calls ``accum_quoted_string``.  The latter accumulates the characters of
    the string, checking for escapes and the like.  When a string literal or
    wide string literal is scanned outside of preprocessing mode, ``get_token``
    will call ``concat_adjacent_string_literals``, which will check if the next
    token is another string literal.  If so, the adjacent strings will be
    concatenated into a single string literal token.  scan_char_constant
    recognizes user-defined character literals and sets
    ud_lit_op_sym_for_curr_token, as appropriate.  In order to handle merging
    of suffixes across concatenated string literals, the similar task for
    user-defined string literals is handled by concat_adjacent_string_literals,
    except when fetch_pp_tokens is TRUE; in that case, get_token performs the
    requisite processing, as string literals are not concatenated in that mode.
* | When a ``#`` appears as the first non-white-space character on a line, a
    call to ``pp_directive`` (in ``preproc.c``) is made to process a
    preprocessing directive.

One very important requirement is that ``get_token`` be fast.  About 30 percent
of total time time in the front end is likely to be spent in ``get_token`` and
its subordinate routines.  Every effort has been made to make those routines
fast, and in particular, to avoid unnecessary subroutine calls.  As an example,
simple white space (blanks and horizontal tabs) is skipped in ``get_token``
itself, rather than incurring the overhead of calling ``skip_white_space``.

The handling for pp-tokens and normal tokens differs in a number of ways, but
the most fundamental of those is the information returned to the caller.  The
kind of token is returned in both cases.  However, for identifiers and literal
constants, where additional information must be returned to precisely specify
the token, the returned value in the pp-token case is the actual characters of
the token, and in the normal token case it is something one step removed from
that: a symbol locator for identifiers, or a constant entry for constants.
This is useful, but it's also necessary -- when scanning normal tokens, and in
particular identifiers and literal constants, there are some cases where it is
necessary to skip forward to the next token to see if it has some effect on the
current one.  For example, after scanning a string literal, one must look ahead
to see if the next token is another string literal, and if so, one concatenates
the two.  Since the next token may be on another line, it's clear that the
original string for the token in the current source line may be gone by the
time the next token is reached.  Therefore, the locator or constant entry form
is necessary even inside of ``get_token``.

Token Lookahead and Caching
---------------------------

Sometimes syntactic analysis requires lookahead -- it requires the fetching
of one or more tokens past the current position to discriminate between two
possible parses.

This comes up even in C, although the lookahead there is limited to a single
token.  For example, when a statement begins with an identifier, one must look
past the identifier to see whether or not the next token is a "``:``" -- if so,
the identifier is a label.  (The routine ``next_token`` provides this
capability.)

In C++, lookahead is needed in many more contexts, and is not limited to
one-token lookahead.  In addition, there are some language constructs that must
be saved in some lexical form when they appear, and then parsed later on the
basis of the saved information.  The most obvious example is the bodies of
member functions declared inside a class: their name binding cannot be done
until the entire class declaration has been scanned, so the easiest
implementation is to save the member function bodies and then parse them when
the closing brace of the class is reached.

A construct called a *token cache* supports all these needs for lookahead, as
well as recording the tokens of template definitions and other syntactic
elements for delayed use.  a_token_cache is a data structure that represents a
sequence of tokens.  The tokens in a cache are represented by a list of shared
tokens (a_shared_token), each one describing an immutable token state (this
includes not just the token kind, but also any additional information required
to fully specify the token, e.g., a symbol locator for an identifier or a
constant entry for a literal constant).  A similar data structure,
a_reusable_token_cache, is used for the tokens of a template definition or
delayed construct.

To build a nonreusable token cache, one declares a variable of type
a_token_cache.  Then one reads through tokens in the usual way, calling
get_token to fetch each token, and then calls cache_curr_token before advancing
to the next.  When enough lookahead or caching has been done, one can put all
the tokens in the token cache back on the front of the input stream by calling
rescan_cached_tokens.

get_token does not read directly from nonreusable token caches (although it
does do so with reusable token caches; see below).  Instead,
rescan_cached_tokens adds the current token to the end of the cache and then
pushes the cached tokens in LIFO order onto the global
cached_token_rescan_stack (implemented as a stack instead of a list for
efficiency reasons), and finally calls get_token to fetch what was the first
token of the cache from the cached token rescan stack as the current token.  As
long as the global rescan stack is non-empty, get_token will fetch a token from
the rescan stack (by calling get_token_from_cached_token_rescan_stack) instead
of scanning a new token from the current source line.

``next_token`` is a simple user of the token caching facility; it caches the
current token, fetches the next and remembers its kind, then pushes the cache
back on the input stream to restore the original token (and also the next one,
behind it).

``unget_token`` puts the current token back on the front of the input stream,
so that in effect afterwards there is no current token (although it doesn't
actually go so far as to change ``curr_token``).  It is used in some unusual
error situations that want to fabricate and insert an identifier token where
one was missing in the source.

Two routines are available to cache sequences of tokens delimited by given
tokens:

* ``cache_token_stream_until_matching_token``
* ``cache_token_stream_with_coalesce_flag``

``cache_token_stream_with_coalesce_flag`` is not intended to be called
directly.  Instead, it is called by one of two interface routines:

* ``cache_token_stream``
* ``cache_token_stream_coalesce_identifiers``

``cache_token_stream`` is used to cache tokens until one of a specified set of
stop tokens is found.  This is used, for example, when caching the body of a
class or function template when all tokens up to a closing brace should be
cached.  In such situations, the only special processing that is needed is to
keep track of paired delimiter tokens (e.g., ``{}``, ``[]``, ``()``) so that,
for example, the closing brace of an inner block is not incorrectly taken as
the end of the template body.

When the stop token could appear within a template argument list, additional
processing must be done to determine whether a "``<``" that is encountered is
the start of a template argument list or simply a less than sign.  In order to
determine this, the identifiers found while the tokens are cached must be
coalesced (the identifier coalescing process is described later in this
chapter).  This special kind of token stream caching is done by calling
``cache_token_stream_coalesce_identifiers``.  The process of coalescing an
identifier involves fetching some number of tokens.  Because these tokens are
not fetched directly by ``cache_token_stream_coalesce_identifiers``, a
mechanism is needed to get access to those tokens.
``begin_caching_of_fetched_tokens`` is called to request that a cache of
fetched tokens be automatically constructed.  Each new token returned by
``get_token`` is added to a special cache.  The token stream is then cached by
recording the starting token sequence number, getting each token, and
coalescing each identifier that is encountered until the stop token is found.
Once the starting and ending token sequence numbers are known, the tokens in
that range are extracted from the special cache.
``end_caching_of_fetched_tokens`` is then used to indicate that the automatic
caching of tokens is no longer needed.

As noted above, token caches are also used to represent template definitions
and other syntactic constructs that require delayed processing.  The tokens
that comprise the syntactic element are cached when it is scanned and are
rescanned later when needed.  While other token caches are rescanned once and
discarded, reusable token caches may be rescanned over and over again.
Reusable token caches should be constructed by calling the factory function
shared_obj<a_token_cache> with reusable set to TRUE (this acts as a sanity
check to ensure the token cache is used in the intended way, but they are
otherwise built in exactly the same manner as other token caches).  The
reusable token cache object (of type a_reusable_token_cache) is then
constructed from the result.

Various scenarios can cause multiple reusable token caches to be pending rescan
at a given time; calling rescan_reusable_cache pushes a reusable token cache
onto the global reusable_cache_stack.  get_token first rescans tokens from the
(nonreusable) cached token rescan stack, as described above.  When this stack
is exhausted it rescans tokens from the reusable cache at the top of the
reusable cache stack by calling get_token_from_reusable_cache_stack.  When all
of the entries in a reusable cache have been scanned the cached token rescan
stack is restored and the entry is popped off of the reusable cache stack.
Token scanning then resumes with the next token on the cached token rescan
stack.

Reusable and nonreusable token caches may be freely intermixed; template
instantiations may occur while scanning tokens from a cache and a template
instantiation will usually create new caches, as needed, when doing
look-ahead, as described earlier.

Source Positions
----------------

Each time a token is fetched, the global variable ``pos_curr_token`` is set to
the source position of the first character of the token.  A source position
means a sequence number (which can be converted to a file name and line number)
and a column number from the original source line.  The position is determined
by the macro ``macro_line_loc_to_source_pos``.  In the simplest and most common
case, a pointer to the starting character of the current token can be converted
easily to a source position by using the current sequence number and
determining the column number by taking the difference between the pointer and
the start of ``curr_source_line`` and applying an offset to convert the byte
offset into a logical column number using the macro ``logical_column_offset``
(which can in some cases result in a call of ``f_logical_column_offset``).  In
more complicated cases, ``conv_line_loc_to_source_pos`` is called, and it uses
the information in both ``orig_line_modif_list`` and ``source_line_modif_list``
to determine the source position.  The source position of something in a
preprocessing modification is taken to be the position of the first character
of the original source line that contains the macro invocation that leads to
the given modification.

The Input Stack
---------------

``open_file_and_push_input_stack`` is called to enter both the primary source
file and ``#include`` files -- by ``fe_init`` (in ``fe_init.c``) and by
``proc_include`` (in ``preproc.c``), respectively -- and it in turn calls
``open_file_for_input`` and ``push_input_stack`` to do the work; when end of
file is reached, ``pop_input_stack`` is called.

``open_file_for_input`` loops to try each possible include file directory when
attempting to open an include file; it also contains code for the (optional)
implicit inclusion feature of automatic template instantiation, permitting a
series of suffixes to be tried in each directory.

Together, the push/pop routines manage the ``input_stack``, which contains
information about the current nesting of input files.  To limit the number of
open files, they will reuse files once a few files have been opened (see
``MAX_INCLUDE_FILES_OPEN_AT_ONCE``).  The storage for ``input_stack`` is
dynamically allocated and is reallocated larger as needed.

If the options requesting ``makefile`` information or a list of header files
were specified on the command line, ``do_preprocessing_only`` will be TRUE but
``generate_pp_output`` will be FALSE.  Either ``list_makefile_dependencies`` or
``list_included_files`` will be TRUE.  ``push_input_stack`` writes out the
appropriate information to the preprocessing output file.

The front end recognizes certain idioms used to allow an include file to be
included more than once with all of the includes after the first having no
effect.  When one of these idioms is recognized the front end will ignore
subsequent attempts to include the file again.  The front end recognizes two
commonly used mechanisms used to allow a file to be included multiple time:
placing the entire contents of a file inside a ``#ifndef`` or ``#ifdef``, and
use of the ``#pragma once`` directive.  A simple state machine is used to
determine whether subsequent includes of a given file may be suppressed.  The
state information is maintained in the input stack entry.
``suppress_subsequent_include_of_file`` is called for each file to be included.
If it returns TRUE then the call of ``push_input_stack`` is skipped and the
include is suppressed.  See the comments that precede ``find_include_history``
for additional information.

Skipping White Space
--------------------

``skip_white_space`` is called to skip over any white space and get to the
start of the next token (some simple cases of white space are skipped in
``get_token``, for efficiency).  Besides the usual white-space characters, it
also handles the skipping of comments, calling ``read_logical_source_line`` to
fetch the next line of input, and going into and leaving source modifications
upon encountering lexical escape sequences (e.g., ``ATTENTION_MARKER`` to go
into a modification, ``LE_ESCAPE/LE_END_OF_INSERTION`` to leave a
modification).

``skip_white_space`` tracks the kind of white space it has skipped over
(comment/other) and sets global variable ``kind_of_white_space_skipped``.  This
information is useful in the macro processing routines, where white space can
be significant.  Note that ``kind_of_white_space_skipped`` is not necessarily
set correctly if one calls ``get_token``, since ``get_token`` does some
white-space skipping on its own.

When ``skip_white_space`` recognizes one of the ``lint`` comments

  | ``/*ARGSUSED*/``,
  | ``/*NOTREACHED*/``,
  | ``/*VARARGS*/``, or
  | ``/*VARARGS`` *n*\ ``*/``

``add_curr_token_pseudo_pragma`` is called.  It adds a pending pragma entry to
the current token pragma list.  Later, the pending pragma entry is removed via
a call to ``extract_specific_pragmas``, and processed (by setting global state
information or flags in IL entries).  For ARGSUSED and VARARGS, that's done in
``record_lint_argsused_and_varargs_state``.  For NOTREACHED, it's done in
``check_lint_notreached_state``.

Hanging Delete
--------------

If global variable ``delete_source_from_loc`` is non-NULL, the text that
extends from that location in the current line to the position immediately
preceding ``curr_char_loc`` is to be deleted.  This is referred to as a
"hanging delete", and is used in deleting macro invocations.  This flag is
necessary because the complete text of a macro invocation must be deleted (to
be replaced by the expansion of the macro), but the macro invocation may span
several lines, and the macro routines are not in control when ends of lines are
passed in ``skip_white_space``.  By setting this flag, the macro routines can
request that the text of a macro invocation be correctly deleted even if it
spills over onto additional lines.  When the deletion is handled at line end,
``delete_source_from_loc`` is updated to point to the start of the new source
line so that the hanging delete remains in effect.

Whitespace keywords
-------------------

C++/CLI has "whitespace keywords," which are keywords made up of two words
separated by whitespace but considered a single keyword, for example "``ref``\
``class``".  These are scanned by ``scan_whitespace_keyword``.  One interesting
detail is that the characters that make up the token (which can be spread over
multiple lines because of the embedded whitespace) are replaced by a canonical
spelling of the keyword containing a single embedded space, the same way that a
macro is replaced in the input line by its expansion.  That allows processing
that deals with tokens on the pp-token level to handle those whitespace keyword
tokens like any others, as a contiguous sequence of characters.

Textual Preprocessing Output
----------------------------

When compiling C to intermediate language, there is no need to assemble the
actual logical source line that results after preprocessing.  All that is
required is that the tokens of that line can be extracted.  But when
preprocessing only is being done, the preprocessed line is output from
``curr_source_line`` and the information in ``source_line_modif_list``.  [#f1]_
``gen_pp_output_for_curr_line`` assembles and puts out the effective line,
calling ``gen_pp_line_info`` to generate line-identification directives when
the file/line of the current line does not immediately follow that of the line
previously output.  ``gen_pp_output_for_curr_line`` is called by
``read_logical_source_line`` right before a new line is read; it is also called
when the input stack is pushed and popped for the starts and ends of input
files.

There are some special problems in producing the preprocessing output line when
preprocessing directives are passed to output.  This must be done for certain
directives (like ``#pragma``) that the preprocessor cannot process.  If a
directive of that type contains a multi-line comment, the parts of the
directive preceding and following the comment must be written out on the same
line.  In this case, the part preceding the comment will be written without a
terminating newline, and the part following the comment will be written later
on the end of that line.

Reading Logical Source Lines
----------------------------

Aside from its primary function of reading the next logical source line and
processing trigraphs and line splices, ``read_logical_source_line`` also calls
``gen_pp_output_for_curr_line`` to output the previous line (if needed), clears
the modification lists, and handles ends of files.  It can be told to stay at
the end of a file (rather than popping out to the enclosing file) on end of
file, if that is desirable (as it is when scanning a comment that is unclosed
at end of file).  On the ultimate end of file, it returns a special end-of-file
line (containing just an ``LE_END_OF_LINE`` lexical escape sequence) in
``curr_source_line``, and sets ``after_end_of_all_source``.

If ``curr_source_line`` is too small, ``expand_curr_source_line`` is called to
reallocate it, and ``adjust_curr_source_line_structure_after_realloc`` (in
``macro.c``) is then called to walk through the data structure associated with
``curr_source_line`` and change any pointers to ``curr_source_line`` to point
to the proper addresses within the new allocation for ``curr_source_line``.

Multibyte characters in source code
-----------------------------------

When ``MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED`` is TRUE, multibyte character
sequences are accepted in comments, string literals, character constants,
identifiers, and header names.  Multibyte character sequences are not wide
characters (e.g., Unicode); they are sequences of one or more characters,
appearing in a string of characters of the usual ``char`` size, and
representing single logical characters.  For example, in the Japanese SJIS
encoding, the sequence 82A9 (in hexadecimal) represents a single Kanji
character: the first character (82) is in a range that indicates it is the
first character of a two-character sequence.  Typically, the characters in the
ASCII code set will be accepted as themselves in multibyte character encodings,
i.e., they will be sequences of length one.  Since each character sequence in a
multibyte character string need not have the same length (and some encodings
have explicit shift-state-change characters), a multibyte character string can
be understood only by scanning it from beginning to end, advancing over one
logical character at a time.

Moreover, since characters after the first in a multibyte sequence can look
like simple ASCII characters, it is not generally okay to look for an ASCII
character by doing a simple ``strchr``-type search.  The lexical routines count
on being able to do this for only two characters: the zero/null character (this
is guaranteed by the C standard; characters after the first in a multibyte
character sequence are not allowed to be zero) and the newline character (this
is effectively guaranteed by the fact that one must be able to read files
containing multibyte character sequences and discern the ends of lines without
tracking the multibyte character state as each character is read).  [#f2]_

To advance over logical characters, one needs a routine that will determine the
length of the multibyte character sequence at a given location.  This is done
by the macro ``mbc_length`` and routine ``f_mbc_length``.  In the usual
configuration, that routine is implemented by calling the standard C library
routine ``mblen``.  ``mbc_length`` and related routines are used to do special
processing in the following places:

* | In ``read_logical_source_line``, the line splice ("``\``") and trigraph
    ("``?``") characters are ignored if they appear in the middle of a
    multibyte character sequence.
* | In ``skip_white_space``, the closing character for a C-style comment
    ("``*``") is ignored if it appears in the middle of a multibyte character
    sequence.  ("``//``" end-of-line comments require no special handling,
    because the end-of-line marker is rendered as a lexical escape
    sequence,which cannot appear within a multibyte character sequence.)
* | In ``accum_quoted_string``, ``scan_char_literal``, ``scan_string_literal``,
    ``conv_single_char``, and ``conv_single_wide_char``, the characters inside
    quoted strings are scanned as multibyte characters as necessary.  The
    routine ``mbc_to_wide_char`` is used to convert a multibyte character
    sequence in a wide string literal or wide character constant to the
    appropriate ``wchar_t`` character.  In the usual configuration,
    ``mbc_to_wide_char`` is implemented by calling the C library routine
    ``mbtowc``.
* | In ``get_token``, the scanning for identifiers uses ``is_identifier_char``
    to recognize identifier characters beyond the basic character set.
    ``make_canonical_identifier`` handles encoding the multibyte characters
    appropriately in the name string for the identifier.

Note that not all source code is scanned as multibyte characters, because that
would take too much time.  The special scanning is done only in the contexts
listed above.  For the line-splice and trigraph cases, when the line-read
routine sees those characters it goes to the beginning of the line and steps
through the characters of the line to find out where the character falls in the
multibyte character sequences, but that processing is done only if a potential
line splice or trigraph appears.  Note also that there are configuration
options to turn off the special processing for several of the special
characters, for cases where the encoding guarantees no confusion on the
particular character.

If the encoding has locking state-change characters (e.g., JIS), each C-style
comment and string is assumed to begin in the initial shift state.  This
follows a requirement stated in 5.2.1.2 of the ANSI/ISO C standard

If ``UNICODE_SOURCE_SUPPORTED`` is TRUE, the multibyte encoding selected is the
UTF-8 encoding of Unicode.  UTF-16 is also supported, in the little-endian or
big-endian format, and is converted immediately to UTF-8 on input.  The type of
source code in a file can be specified by a byte order mark (BOM) at the start
of the file, or by way of the ``--unicode_source_kind`` command-line option.
There is also a macro ``DEFAULT_UNICODE_SOURCE_KIND`` that can be used to
establish the default for files without a byte order mark.

When ``UNICODE_SOURCE_SUPPORTED`` is set to TRUE, identifiers and file name
strings in the front end are represented in UTF-8.  For file names, there is
the expectation that the standard system fopen will be able to deal with UTF-8
file names.  If that isn't the case, some host-specific code may have to be
added to ``fopen_interface``.  (For Windows-hosted configurations, specific
code is provided which calls the ``_wfopen`` function.)

If UTF-8 identifier name strings are somehow undesirable (e.g., a back end or
linker can't handle them), the macro
``IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS`` can be set to FALSE, in which case
characters in identifiers outside of the basic character set will be rendered
in the ``\u``\ *xxxx* or ``\U``\ *xxxxxxxx* form also used for UCNs.  Note,
however, that the encoded form will come out in diagnostic messages that name
an entity in the source.

If ``DEFAULT_UNICODE_SOURCE_KIND`` is ``usk_none``, source files without a byte
order mark are assumed to be encoded in ISO-8859-1 (otherwise known as
Latin-1), which matches the first 256 code points in Unicode.
(``LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED`` should be set to a locale that
matches ISO-8859-1.) Such a mode (which is the default for Windows-hosted
versions) is a hybrid mode in which some files are Unicode and some are not,
and in which therefore a character like a u-umlaut would be encoded differently
depending on the kind of source file it appears in.  Such characters in
non-Unicode files are converted to UTF-8 (or the identifier encoding ``\u``\
*xxxx*) when they appear in identifiers and file names.  In hybrid
configurations, characters in textual preprocessing output are always converted
to UTF-8 if ``DEFAULT_UNICODE_SOURCE_KIND`` is not ``usk_none``, and to
ISO-8859-1 otherwise.  Characters that cannot be represented in ISO-8859-1 in
the latter case are converted to "``?``".

When ``UNICODE_SOURCE_SUPPORTED`` is TRUE,
``NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE`` may be set to TRUE to
indicate that non-Unicode files are encoded in some character set other than
ISO-8859-1.This feature requires the ability to translate other character sets
to and from Unicode.  Such a facility is provided in newer versions of the
Windows API (Microsoft version 1400 and above) so this feature is enabled when
the front end is built with ``EDG_WIN32`` set to TRUE.  In identifiers and file
names, native characters are translated to their UTF-8 equivalent.  In wide
strings, native characters are translated to their Unicode equivalent.  In
narrow strings, they are left in their original form.

When Microsoft extensions are enabled and ``EDG_WIN32`` is TRUE, this feature
enables support for the Microsoft setlocale pragma, which specifies the
character encoding to be used for the translation to and from Unicode.

Raw Listing Output
------------------

``gen_raw_listing_output_for_curr_line`` is called to write source lines to
``f_raw_listing`` when raw listing information is requested; it calls
``gen_expanded_raw_listing_output_for_curr_line`` to write out the
macro-expanded form of the source line if the source line has some
modifications.

``gen_rlisting_line_info`` writes information about transitions into and out of
include files; it is called by ``push_input_stack`` at beginnings of files and
by ``pop_input_stack`` at ends of files.

Inserting Text into the Token Stream
------------------------------------

``insert_string_into_token_stream`` may be called to cause a string to be
scanned as tokens that are processed as part of the input stream either before
or after processing the current token.  When scanning tokens from the string,
macros are optionally expanded and adjacent string literals are concatenated.
The string cannot contain preprocessing directives, and trigraphs are not
recognized.

This mechanism is used is used to internalize C++ code generated by the C++
metadata reader (see :ref:`cppcli-assembly-decls`) in the functions
``scan_top_level_metadata_declarations`` and ``get_definition_of_class``.

Utility Routines
----------------

There are several routines in ``lexical.c`` that are utility routines for
syntax analysis, and are not directly related to ``get_token``:

* | ``flush_tokens`` is called on the occurrence of a syntax error, and throws
    away tokens (by calling ``get_token``) until a token is found that seems an
    appropriate place to restart.  The current stop token set pointed to by
    ``curr_stop_token_stack_entry`` contains the set of tokens on which to stop
    the flush (it is maintained by the macros ``add_stop_token``,
    ``remove_stop_token``, and the routines ``push_stop_token_stack`` and
    ``pop_stop_token_stack``).  Recursive-descent routines place the tokens
    they expect in the set of stop tokens; then, if an error occurs, the flush
    will stop at a point that one of the recursive descent routines will
    recognize.  ``flush_tokens`` should not in general be called directly;
    ``syntax_error`` should be called to report a syntax error, and it will
    call ``flush_tokens``.
* | ``flush_until_matching_token`` is called by ``flush_tokens`` (and can be
    called from elsewhere) to flush from an opening token such as a left
    parenthesis to the matching closing token.
* | ``loop_token`` is a utility routine that can be called at the bottom of a
    loop for a repeated syntactic construct (e.g., a comma-separated list).  It
    tests for a particular token and takes it if it is found.
* | ``required_token`` is a utility routine that should be called when a
    particular token is required to appear next.  It checks for that token, and
    issues an error if the token is not found.
* | ``next_token`` returns the next token after the current one, without
    disturbing the current token.  This is used in recursive descent parsing
    routines to look ahead at the next token to decide between two paths
    through the syntax.
* | ``next_two_tokens`` is similar to ``next_token`` except that if the next
    token matches a value supplied by the caller the token after the next token
    is fetched and returned to the caller.  This is used when scanning vacuous
    destructor references in C++ where it is necessary to look for a tilde
    following a ``::`` to recognize a nonclass vacuous destructor reference
    [#f3]_.
* | ``push_lexical_state_stack`` and ``pop_lexical_state_stack`` are used to
    enter and exit a new lexical context.  These are used, for example, when
    instantiating a template and when switching from fetching normal tokens to
    scanning a preprocessing directive.

Scanning Names in C++
=====================

Unlike C, in which all names are simple identifiers, in C++ the names of
entities are often more complicated.  Names may be preceded by one or more
qualifiers such as namespace and class qualifiers (e.g., ``A::B::i``) and
global scope qualifiers (e.g., ``::i``); class qualifiers may include template
references (e.g., ``A<int>::i``); and the name that follows these qualifiers
may be more complicated than a simple identifier: it may also be an operator
name (e.g., ``operator+``), a conversion function name (e.g., ``operator
int``), a (C++11) literal operator name (e.g., operator ``""``\ _km), or a
destructor name (e.g., ``~A``).  An arbitrarily complex name is referred to as
a "generalized identifier".  The process of collecting the tokens of a
generalized identifier and replacing them with a single ``tok_identifier``
pseudo-token is referred to as "coalescing" the generalized identifier.  A
hierarchy of routines is provided that scan generalized identifiers:

* | ``is_generalized_identifier_start`` determines whether the current token
    begins a generalized identifier and, if so, scans the tokens that comprise
    the generalized identifier and records its description in
    ``locator_for_curr_id``.
* | ``coalesce_and_lookup_qualified_name`` calls
    ``is_generalized_identifier_start`` and, if the generalized identifier
    includes a qualifier, the name after the qualifier is looked up in the
    scope specified by the qualifier.  Names without qualifiers are not looked
    up.  [#f4]_
* | ``coalesce_and_lookup_generalized_identifier`` is used to scan and look up
    a generalized identifier.  It calls ``coalesce_and_lookup_qualified_name``
    and looks up the name if it does not include a qualifier.  Names that
    include qualifiers will have already been looked up by
    ``coalesce_and_lookup_qualified_name``.

The arguments to these routines include options used to specify the kinds of
names that may be accepted and, for the coalesce-and-look-up routines, a lookup
mode that specifies the type of name lookup to be performed.  The ``options``
parameter specifies a set of "generalized identifier" (GID) flags.  The GID
flags specify several different types of information:

* | The kinds of identifiers that should be recognized.  For example, ``~A``
    may be recognized as a destructor, or the tilde may be an operator.
* | The kinds of identifiers that are allowed.  For example, are qualified
    names allowed in this context? Are operator names allowed? The difference
    between constructs that are "recognized" and those that are "allowed" is
    that constructs that are not recognized (i.e., ``~D``) are not considered
    identifiers by ``is_generalized_identifier_start`` while constructs that
    are not allowed are considered identifiers but cause errors to be issued
    indicating that the particular type of identifier is not permitted in the
    current context.

.. _recognizing-generalized-identifiers:

Recognizing Generalized Identifiers
-----------------------------------

``is_generalized_identifier_start`` is the lowest-level generalized identifier
routine and does most of the work.  As the name suggests,
``is_generalized_identifier_start`` is used to determine whether the current
token is the beginning of a generalized identifier.

``is_generalized_identifier_start`` is actually a macro that first checks
whether the current identifier has already been coalesced.  If it has, the
macro immediately returns TRUE; otherwise it calls
``f_is_generalized_identifier_start``, which performs the rest of the
processing described here.

For these examples ``is_generalized_identifier_start`` returns TRUE and sets
the current token to ``tok_identifier``:

   | ``i::iX::i``
   | ``::X::i``
   | ``::A::B::i``
   | ``A<int>::i``
   | ``A::operator =``
   | ``A::operator int``
   | ``A::operator ""_km // When user-defined literals are recognized``
   | ``A::~A``
   | ``NS::i``
   | ``NS::A::i``
   | ``operator =``
   | ``operator int``
   | ``operator ""_km    // When user-defined literals are recognized``
   | ``~A                // When destructors are recognized``
   | ``A<int>``
   | ``NS::A<int>``
   | ``~int              // When nonclass vacuous destructors are recognized``
   | ``int::~int         // When nonclass vacuous destructors are recognized``
   | ``A::``\ *anything else except* ``*   // Error``

For these examples FALSE is returned and current token is set to
``tok_ptr_to_member``:

   | ``X::*``
   | ``A<int>::*   // Template reference will be coalesced``

For these examples FALSE is returned and the current token is not changed:

   | ``::new``
   | ``::delete``
   | ``~A          // When destructors are not recognized``
   | *anything else*

Determining whether or not the current token is the start of an identifier may
involve scanning all of the tokens that comprise the identifier.  Consequently,
all of the tokens that make up an identifier are scanned the first time that
``is_generalized_identifer_start`` is called for a given identifier.  The
information needed to describe the generalized identifier is recorded in the
symbol locator, and on subsequent calls the information from the locator is
used to return the appropriate value to the caller.  When a sequence of tokens
is coalesced the original tokens are replaced with a single ``tok_identifier``
or ``tok_ptr_to_member``.  The resulting token and related state information in
the locator is referred to as a "pseudo-token".  When
``is_generalized_identifier_start`` determines that the thing being scanned is
an identifier, the current token is set to ``tok_identifier``, the locator is
updated with a description of the identifier, and the return value of the
function is TRUE.  The fields in the locator that are set are

* | ``is_qualified_name``
* | ``is_global_qualified_name``
* | ``is_file_scope_qualified_name``
* | ``is_operator_name``
* | ``is_destructor_name``
* | ``is_udl_operator_name``
* | ``is_class_member``
* | ``parent``
* | ``has_been_coalesced``
* | ``is_vacuous_destructor_reference``
* | ``is_nonclass_destructor``

When coalescing a qualified name such as ``A::xxx`` the result is a locator
whose symbol header points to the name ``xxx``, and whose ``parent`` points to
the ``A``.  If ``A`` is a class, ``is_class_member`` is TRUE and
``parent.class_type`` points to the type of ``A``.  If ``A`` is a namespace,
``is_class_member`` is FALSE and ``parent.namespace_ptr`` points to the
namespace entry for ``A``.  The final identifier (``xxx``) is not looked up.
Instead, the information is preserved in the locator so that, if necessary,
``xxx`` can be looked up by ``coalesce_and_lookup_qualified_name``.

``is_generalized_identifier_start`` coalesces template class references of the
form ``A::i``, [#f5]_ ``A<T>``, and ``A<T>::i`` (but not simply ``A``) by
calling ``coalesce_template_id``.  These references must be coalesced to
determine whether the template reference is the beginning of a qualified name
(e.g., ``A<int, float>::i`` cannot be determined to be a qualified name until
the ``::`` is seen.) The result of a template class reference is an incomplete
instantiation.  A complete instantiation is only generated when it is required
for a name to be looked up.  A complete instantiation will be generated for
references like ``A<T>::X::Y``, because ``X`` must be looked up in ``A<T>``.  A
complete instantiation will not be generated (at least not here) for references
like ``A<T>::X``.  The complete instantiation is delayed until ``X`` needs to
be looked up (typically in ``coalesce_and_lookup_qualified_name``.) A template
argument list may also follow a function template name or an overloaded
function name in which one of the members of the overload set is a function
template.  This is known as "explicit specification of function template
arguments".  Unlike template class references, a template function reference
cannot be resolved to a given template instance when the template argument list
is scanned (because function templates can be overloaded).  As a result, when a
function template argument list is scanned, a pointer to the list is retained
in the symbol locator, but the reference is not otherwise resolved at this
point.

The pointer operator used when declaring a pointer-to-member looks a lot like a
qualified name; the only difference is that the final identifier is replaced
with an asterisk (``*``).  For example, a pointer-to-member operator may look
like ``A<int>::B::C::*``.  Because of this similarity pointer-to-member
operators are also coalesced by ``is_generalized_identifier_start`` even though
they are not considered identifiers.  When a pointer-to-member operator is
coalesced the current token is set to ``tok_ptr_to_member``, the
``parent.class_type`` field of the locator points to the type specified in the
operator (e.g., ``A<int>::A::B::C`` in the previous example), and the return
value of the function is FALSE.

The operators ``::new`` and ``::delete`` look like qualified names but are
actually operators.  When called for these operators,
``is_generalized_identifier_start`` leaves the current token unchanged and
returns FALSE.

C++ allows a destructor to be called for classes that have no destructor and
for nonclass types.  We refer to this kind of destructor call as a "vacuous
destructor" reference.  A GID option is used to indicate whether a vacuous
destructor should be recognized and, if so, whether it must be a nonclass
destructor.  Vacuous destructor references, which are only allowed in field
selection operations, differ from other generalized identifiers in several
important ways:

* | the qualifier, if present, and the identifier that follows the tilde may be
    a nonclass type.
* | the nonclass type found may be a keyword that names a basic type (e.g.,
    ``int::~int``).

When a vacuous destructor reference is coalesced, the ``parent.class_type``
points to the type specified by the name *after* the ``::~`` because it must be
possible to record the type of the vacuous destructor reference for cases that
do not include a qualifier.

Coalesce-and-Look-Up Routines
-----------------------------

The coalesce-and-look-up routines are:

* ``coalesce_and_lookup_qualified_name``
* ``coalesce_and_lookup_generalized_identifier``

After ``is_generalized_identifier_start`` has determined that the current token
does in fact begin an identifier the coalesce-and-look-up routines are used to
do the appropriate kind of name lookup.  The difference between the two
routines is that ``coalesce_and_lookup_qualified_name`` will only perform a
lookup of the final identifier of a qualified name while both qualified names
and names without qualifiers will be handled by
``coalesce_and_lookup_generalized_identifier``.

One of the arguments passed to these routines is an "identifier lookup mode",
an enumeration that specifies the kind of lookup to be performed.  The lookup
mode is translated into a set of "identifier lookup" (IDL) flags that are
passed to the symbol table lookup routines.  An error is issued by
``coalesce_and_lookup_qualified_name`` if the name lookup fails, except in
``ilm_tentative_type`` mode.  [#f6]_ In all other respects the various
identifier lookup modes are identical as far as the coalesce and lookup
routines are concerned.  ``coalesce_and_lookup_generalized_identifier`` does
not issue an error if the lookup of an unqualified name fails.

Generalized Identifier Errors
-----------------------------

``check_for_generalized_identifier_errors`` is called by the
coalesce-and-look-up routines and also by ``is_generalized_identifier_start``
to ensure that the identifier that has been scanned meets the constraints
specified by the GID "allowed" flags.  The error tests are done by comparing
the current GID options with the information in the locator.  When one
generalized identifier routine calls another, it masks off the error flags so
that the errors are diagnosed only once by the highest-level routine.

``check_for_template_declarator_errors`` is called by the coalesce-and-look-up
routines to verify that the declarator in a template declaration does actually
refer to a template entity that is permitted in a template declaration.  These
checks are done to provide diagnostics that are more helpful than those that
would result if the identifier were simply returned to ``declarator``.

Template References
-------------------

``coalesce_template_id`` coalesces references to class and function templates.
The kind of reference to be coalesced is determined based on a symbol that is
provided.  If the symbol refers to a function template or an overload set
containing one or more function templates,
``coalesce_template_function_reference`` is called, otherwise
``coalesce_template_class_reference`` is called to handle class template
references or to diagnose the improper use of a template argument list.

Template Class References
^^^^^^^^^^^^^^^^^^^^^^^^^

``coalesce_template_class_reference`` coalesces template class references, such
as ``A<int, 1>``, into identifiers for which the ``specific_symbol`` field of
the locator points to a specific instance of the template.  A class template
name is usually, but not always, followed by a template argument list.  The
argument list may be omitted in declarations of the class template,

.. code:: c++

   template <class T> class A;
   template <class T> class A { /* ... */ };

in which case the name refers to the class template (not to one of its
instances), and may also be omitted inside the definition of a class template
or in the definition of a member function or static data member of the class
template,

.. code:: c++

   template <class T> class A {
     A*       a;    // Equivalent to A<T>*   a
     int A::* pma;  // Equivalent to A<T>::* pma
   };

in which case the name refers to the current instance of the class template
(e.g., ``A<T>``).

There are several contexts in which template class references must be
coalesced.  Most calls of ``coalesce_template_class_reference`` are from
``is_generalized_identifier_start`` and include a template argument list or are
part of a qualified name.  ``coalesce_template_class_reference`` is also called
by ``coalesce_and_lookup_generalized_identifier``, which scans simple class
template names such as ``A``, and by ``scan_tag_name``, which scans specific
definitions of a template classes such as ``class A<int> {}``.  Like the
generalized identifier routines, ``coalesce_template_class_reference`` accepts
a set of generalized identifier options as one of its arguments.  The only
value actually used is ``GID_TEMPLATE_ARGS_OPTIONAL`` which specifies that the
template argument list may be omitted for a reference that is not within the
class template definition.  As described above, the argument list may always be
omitted within the class template definition.

The template argument list, if present, is scanned by calling either
``scan_template_argument_list`` or ``scan_unknown_template_arg_list:``

* | ``scan_unknown_template_arg_list`` is used when scanning the template
    argument list associated with a "nonreal" template -- a class template that
    is a member of a proxy or nonreal class.  A nonreal template is created in
    contexts in which a member class template of a template parameter dependent
    class is used.  For example:

  .. code:: c++

     template <class T> struct A {
       typename T::X<int> tx;
     };

  | In such cases, it is assumed that ``T::X`` is a template because there are
    no cases in which a less than sign could immediately follow a type name, so
    the "``<``" must indicate the beginning of a template argument list.  When
    scanning a nonreal template argument list there is no information available
    about the number of template parameters, their types, etc.  All of this
    must be inferred by scanning the actual arguments.  Consequently, no
    attempt is made to determine whether the number of arguments, the types,
    etc.  are correct.  The disambiguation routines are used to determine
    whether a given argument is a type or nontype (see below for the use of
    this function in scanning function template references.)
* | ``scan_template_argument_list`` is used when scanning the template
    arguments of "real" templates.  Default values are provided for any omitted
    arguments.  If the type of a nontype parameter depends on another template
    parameter, the type of the parameter is generated by rescanning a token
    cache constructed when the template was defined.  Similarly, if the type of
    the default value, or the default value itself, involves a template
    parameter, the default value is also generated by rescanning a token cache
    constructed when the template was defined.

Both routines call ``scan_template_argument_constant_expression`` for nontype
arguments and ``type_name`` for type arguments.  ``find_template_class`` is
called, once a complete template argument list has been created, to find the
type that corresponds to the particular set of arguments.  If no such type
exists, ``find_template_class`` creates one.  In either case the symbol
associated with the type is returned.

Function Template References
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``coalesce_function_template_reference`` coalesces references to functions that
employ an explicit template argument list.  Unlike class templates, function
templates may be overloaded; therefore an explicit template argument list does
not necessarily identify a specific template instance.  The function type of
the instance is also needed to determine the actual instance that is being
referenced, but is not available when the template argument list is scanned, so
the argument list must be retained until some later point when the function
type is also available.

``scan_unknown_template_arg_list`` is used to scan explicit template argument
lists because, in general, there is no template parameter list that can be used
to determine the kind of argument expected (type vs.  nontype), or the type of
the argument (for nontype arguments).  As with nonreal template argument lists,
the disambiguation routines are used to determine whether a given argument is a
type or nontype.  ``type_name`` is used to scan arguments that are types.
``scan_nontype_template_argument`` is used to scan nontype arguments.  Function
template argument lists differ from those of class templates in that, for
function templates, the corresponding template parameter list is not available
when the template argument list is scanned.  The presence of the template
parameter list for a class template makes it possible to convert nontype
arguments to the type of the corresponding template parameter (or to a special
nonreal type in the case of a nonreal argument list).  For function templates,
such conversions cannot be done until the specific function template to be used
has been identified.  Consequently, when a nontype argument is scanned it must
be retained in a form that permits the conversion to be done later.
``scan_nontype_template_argument`` returns a pointer to ``an_arg_operand``, a
data structure used by the expression scanning routines to represent an
expression.  The ``constant_is_an_arg_operand`` field in the template argument
is set to indicate that the nontype argument is represented in this form.

Variable Template References
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``coalesce_variable_template_reference`` coalesces references to variable
templates.  ``scan_template_argument_list`` is used to scan the template
arguments.  ``find_template_variable`` is used to produce a variable symbol for
the instantiation of the given variable template using the specified template
arguments.

Conversion of Literal Constants
===============================

The source file ``literals.c`` contains the code to handle conversion of
literal constants to internal form.  The external form is a string of
characters, a token, as accumulated by ``get_token`` and its subroutines; the
internal form is a structure of type ``a_constant``, representing an integer,
fixed-point, float, character, or string constant.

``conv_integer_literal`` converts decimal, octal, and hexadecimal integer
constants.  Aside from computing the value of the constant, it also recognizes
the trailing ``L`` and ``U`` suffixes, and determines the type of the overall
constant (on the basis of its value and the suffixes).  For ``pcc`` mode, the
rules for determining the type of a constant are modified slightly.

``conv_fixed_point_literal`` converts fixed-point constants (an Embedded C
feature), calling ``fxp_string_to_fixed_point`` or
``fxp_hex_string_to_fixed_point`` in ``fixed_pt.c`` to actually do the
conversion.  It also recognizes the various suffixes (``k``, ``r``, ...), and
sets the type of the result accordingly.

``conv_float_literal`` converts float constants, calling ``fp_string_to_float``
or ``fp_hex_string_to_float_point`` in ``float_pt.c`` to actually do the
conversion.  It also recognizes the ``L`` and ``F`` suffixes, and sets the type
of the result accordingly.

``conv_char_literal`` converts character constants to an integer value.  It
uses ``conv_single_char`` to fetch and convert each character.  If there is
more than one character, they are assembled into one integer (with ordering
controlled by ``TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT``).

``conv_string_literal`` converts string literals.  It receives a count of
effective characters in the string from its caller (as computed by
``accum_quoted_string``), so it can dynamically allocate the space for the
string before scanning it.  It also uses ``conv_single_char`` to convert each
character of the string.

``conv_single_char`` converts one character to an integer value.  This includes
simple characters, escapes like ``\n``, and octal and hexadecimal escapes.  At
the moment, this routine assumes that the host and target character sets are
the same.

``concat_string_literals`` concatenates several string literals (in constant
form in a token cache) into one string.  This is used for the concatenation of
adjacent string literals.

Preprocessing Directives
========================

The source file ``preproc.c`` contains the code to handle preprocessing
directives (except for ``#define``, which is handled in ``macro.c``).
Associated declarations are in ``preproc.h``.

The routine ``cpp_driver`` is the top-level routine when the front end is being
run as a C preprocessor, typically to produce a preprocessed output file.  It
calls ``get_token`` repeatedly until end of source, with
``do_preprocessing_only`` set to TRUE to indicate that preprocessing only --
and not compilation -- is being done.

Normally, when preprocessing output is required, ``generate_pp_output`` is TRUE
as well.  But ``cpp_driver`` is also called when preprocessing is being done to
generate a list of ``#include`` files or a list of ``makefile`` dependencies.
In those cases, ``generate_pp_output`` is FALSE, and either
``list_included_files`` or ``list_makefile_dependencies`` is TRUE.

``pp_directive`` is called (by ``get_token``) when a ``#`` is encountered at
the beginning of a source line.  It sets ``in_preprocessing_directive``,
fetches the directive identifier and identifies it by calling
``identify_dir_keyword`` (which uses a simple linear search), calls the
appropriate routine to process the directive (which may call
``ignore_harmless_trailing_comment`` to ignore extra text at the end of the
directive), calls ``end_of_directive_processing`` (to check for any extra text
that is not harmless), and then resets ``in_preprocessing_directive``.  If a
syntax error is detected in a directive, ``some_error_in_curr_directive`` is
set; this suppresses the check for extra text at the end of the directive.

When the text of a preprocessing directive is not to be written to the
preprocessing output, ``do_not_put_curr_line_in_pp_output`` is set -- initially
in ``pp_directive`` and on each subsequent line, because of multi-line
comments, in ``read_logical_source_line``; the flag is checked by the output
routine, ``gen_pp_output_for_curr_line``.  In the rare case that the text of a
preprocessing directive should be passed to output (because some C compiler
should see the directive to handle it), ``pass_pp_directive_to_output`` is set
to TRUE while the directive is scanned.

Each preprocessing directive has an associated processing routine (for example,
``proc_if`` handles the ``#if`` directive).

``proc_pragma`` processes the ``#pragma`` directive.  This is an area that
would be configured for specific versions.  Unrecognized pragmas must be
ignored (no error should be generated).  When preprocessing only, ``#pragma``
directives are passed to output (see ``pass_directive_to_output``).

``proc_ident`` processes the ``#ident`` directive, although the "processing" it
does is to ignore it.  When preprocessing only, ``#ident`` directives are
passed to output (see ``pass_directive_to_output``).

``proc_error`` passes the rest of the directive to an error reporting routine,
as a catastrophic error, which terminates the compilation.

``proc_line`` scans a line number, treating it as a digit-sequence rather than
an integer constant; optionally, it also scans a file name, calling
``copy_header_name`` to extract the file name from the header name token.  It
puts this information into the input stack and calls routines in ``il.c`` to
remember the proper correspondence between source sequence numbers and file
name/line number pairs.

``proc_include`` scans the file name (as a header name in one of two forms) and
then calls ``push_input_stack`` to request reading from that include file.  It
calls ``copy_header_name`` to extract the file name from the header name token.

``proc_undef`` scans an identifier, looks it up, and removes any macro
definition under that name.  It will not remove predefined macros.  (A
predefined macro has a source position with a sequence number of zero, and a
column number of ``SP_COL_PREDEFINED_MACRO``.)

``#if`` and related directives depend on the ``pp_if_stack``, which is a stack
of entries describing active ``#if`` constructs.  The current depth of the
stack is indicated by ``pp_if_stack_depth``.  Each ``#if`` that has not yet
been closed by a corresponding ``#endif`` appears on the stack.  The stack
entry indicates whether or not the ``#else`` for the ``#if`` has already
appeared, and has the source position of the ``#if`` for use as the error
position if the corresponding ``#endif`` never appears.  In ANSI C or C++ mode,
each ``#if`` is required to be ended in the file in which it was begun: to
implement this, the variable ``base_pp_if_stack_depth`` contains the value of
``pp_if_stack_depth`` as of the beginning of the current source file.  The part
of the stack between ``base_pp_if_stack_depth`` and ``pp_if_stack_depth`` is
considered the only active part of the stack at any given moment.  ``#endif``\
s encountered must be associated with ``#if``\ s in the active part of the
stack, and all of the ``#if``\ s in the active part of the stack must be closed
by the end of the source file (this is checked by
``verify_that_all_pp_ifs_were_closed``, which is called from
``pop_input_stack``).  In ``pcc`` mode, the ``#if``\ s do not have to match up
within each file; they only have to come out right at the end of the
compilation.

The various if-directives are handled by ``proc_if``, ``proc_ifdef`` (which
also handles ``#ifndef``), ``proc_else``, ``proc_elif``, and ``proc_endif``.
``proc_if`` and ``proc_elif`` call ``scan_if_expr``, which calls
``scan_pp_expression`` (in ``expr.c``) to scan the if-expression (there are
restrictions on what can appear in such an expression).  All of the
if-processors call ``perform_if`` once they know whether or not the ``#if``
should be executed; ``perform_if`` returns if the condition is TRUE, and calls
``skip_to_endif`` if the condition is FALSE.  The latter reads lines looking
for preprocessing directives, counts nesting of ``#if``\ s, and looks for an
``#else`` or ``#endif`` that matches the current ``if``.

A new preprocessing directive can be added by:

* | Adding an enumeration constant to ``a_pp_directive_kind``;
* | adding a test for the directive keyword in ``identify_dir_keyword``;
* | adding a switch clause for the directive in ``pp_directive``; and
* | adding a routine ``proc_``\ *directive* to process the directive.  Note
    that the routine must take all the tokens in the directive (up to the
    ``tok_newline``) before returning.

AT&T Preprocessing Extensions
-----------------------------

When ``ATT_PREPROCESSING_EXTENSIONS_ALLOWED`` is defined some of the AT&T
System V release 4 extensions to the preprocessor are supported.  In that case,
the ``#assert`` and ``#unassert`` directives are handled by ``proc_assert`` and
``proc_unassert``.  The data structure built for assert predicates is a simple
one: ``assert_predicates`` points to a list of ``an_assert_predicate`` entries,
each of which gives a predicate name and a list of ``an_assert_value`` entries
giving the value or values for the predicate.  Entry and lookup are done with a
simple linear search.  The token sequences involved are processed into
character strings (containing the token characters with a blank after each
token, and ended with a null character).  Such character strings then represent
the token sequences, and can be saved and compared.
``scan_assert_predicate_reference`` is called from ``get_token`` on detection
of a ``#`` inside of an ``#if`` expression; it scans the reference and returns
1 or 0 depending on whether or not the predicate is defined with the indicated
token sequence.  ``enter_assert_predicate`` can be called from ``fe_init.c`` to
enter predefined predicate names.

C++/CLI Assembly Import
-----------------------

The front supports the C++/CLI ``#using`` directive to import the metadata of
an assembly.  This is done in function ``proc_using``.  In C++/CLI mode, a
"core library assembly" (usually, ``mscorlib.dll``) is imported before input
source is processed, as well as any assemblies specified with the
``--preusing`` command-line option: Function ``process_preusings`` handles that
part (see also :ref:`cppcli-assembly-decls` and :ref:`cppcli-init`).

Macro Processing
================

``macro.c`` contains the code to handle the ``#define`` preprocessing directive
and invocations of macros.  ``macro.h`` contains associated declarations.

Macro Definition Data Structure
-------------------------------

When a macro is defined, the symbol created for it points to an entry of type
``a_macro_def`` (defined in ``symbol_tbl.h``).  The macro's parameter list is
represented by a linked list of entries of type ``a_macro_param``; these give
the names of the parameters (needed only to verify that, on a benign
redefinition of the macro, the parameter names are the same).  The replacement
text of the macro is a sequence of sections, each of which indicates text that
should be placed in the macro expansion at that point.  Each section begins
with a byte that identifies the kind of section, as follows:

.. list-table::

   * - | ``rt_null`` (0)
     - | marks the end of the replacement text.
   * - | ``rt_text``
     - | indicates raw text that is part of the macro replacement text.  The
         next three bytes contain the count of characters following.
   * - | ``rt_raw_argument``
       | ``rt_right_raw_argument``
     - | indicate that the raw (non-macro-expanded) text of an argument is to
         be inserted at this point.  This results from use of the ``##``
         operator, or from parameters scanned in ``pcc`` mode.  The next three
         bytes contain a number indicating the argument to be used (1 is the
         first argument, 2 the second, etc.).  ``rt_raw_argument`` is used for
         a parameter to the left of ``##`` and for all parameters in pcc mode,
         and ``rt_right_raw_argument`` is used for a parameter to the right of
         ``##``.
   * - | ``rt_stringized_raw_argument``
       |
     - | indicates that the "stringized" text of an argument is to be inserted
         at this point (this results from use of the ``#`` operator).  The next
         three bytes contain a number indicating the argument to be used.
   * - | ``rt_argument``
     - | indicates that the macro-expanded text of an argument is to be
         inserted at this point.  The next three bytes contain a number
         indicating the argument to be used.

The ``##`` (paste) operator has no direct representation in the replacement
text; it just results in the two entities being placed next to each other.  For
example,

.. code:: c++

   #define x(a,b) a ## b

results in ``rt_raw_argument(1)`` followed by ``rt_right_raw_argument(2)``.

White space in the replacement text is standardized.  Any white space preceding
or following the replacement text is removed, as is any white space preceding
or following a ``##``, and any white space following a ``#``.  All other white
space is replaced by a single space.

Each token in the raw text (except the last, and any token preceding a ``##``)
is followed by an ``LE_END_OF_TOKEN`` lexical escape sequence.  The raw text
immediately following insertions of arguments (except immediately following a
``##``) also starts with an end-of-token marker, to terminate the last token of
the inserted argument text.  The end-of-token markers ensure that the tokens
will be tokenized the same way each time.  For example, one problem comes up in

.. code:: c++

   #define z(a) ..##a

The value of ``z(.)`` should be three "``.``" tokens, not the one token
"``...``".  End-of-token markers are not used when in pcc mode, since ``cpp``
does not operate on a token level.

Similar standardization of white space and insertion of end-of-token markers is
done when the raw form of macro arguments is accumulated.  This standardization
(of replacement text and macro argument text) makes it easier to check for
benign redefinitions and to generate the stringized form of arguments.

The Macro Buffer
----------------

The global variable ``macro_buffer`` points to a large character array used as
a work area during macro processing.  During macro definition, it holds the
replacement text.  During macro invocation, it holds the expansions of all
macro invocations in the current line.  It is dynamically allocated, and can be
expanded as necessary by calling ``expand_macro_buffer``.
``expand_macro_buffer`` will, in turn, call
``adjust_curr_source_line_structure_after_realloc`` to walk through the data
structure associated with ``curr_source_line`` and change pointers to
``macro_buffer`` to reference addresses within the new allocation for
``macro_buffer``.  ``adjust_curr_source_line_structure_after_realloc``
automatically finds such pointers in global variables and the public data
structure, but it has no way of knowing about local pointer variables.
Therefore, routines that have such variables register them by calling
``register_pointer_variable``, so that they will be found and adjusted if
reallocation is done.

In order to reduce memory consumption when processing very complex and lengthy
macro expansions, the reallocation process for ``macro_buffer`` differs from
that of the other extensible buffers.  While the other buffers are simply
reallocated, relying on the C runtime to copy the complete old contents to the
new allocation, ``expand_macro_buffer`` compacts the contents of
``macro_buffer`` by explicitly allocating a new buffer, copying data to the new
allocation only if it might still be in use, and then freeing the old buffer.
First, each existing source line modification (see
:ref:`lex-src-line-modifications`) is processed to copy its inserted text.
Within that text, only the ``ATTENTION_MARKER`` is copied from each portion
that is marked for deletion by another source line modification.  (The
``ATTENTION_MARKER``\ s must be copied in order to allow scanning for inert
macro names, as described in :ref:`macro-invocations`.) As a result, the
original text of already-expanded macro invocations and the expanded text of
macro arguments will not occupy space in the new ``macro_buffer`` allocation.

Second, text that is in the process of being constructed at the end of the
``macro_buffer`` is copied to the end of the data in the new allocation.  Any
routines that build data structures incrementally, so that reallocation can
occur before the structure is complete (e.g., macro replacement text in
``proc_define``), must identify the beginning of such data using
``begin_macro_buffer_region`` and its completion with
``release_macro_buffer_region``.

Macro Definitions
-----------------

Macro definitions (``#define`` directive) are handled by ``proc_define``.  It
scans the macro identifier, and looks it up in the symbol table (predefined
macros may not be redefined, and normal macros may be redefined only if the new
parameters and text match the existing parameters and text -- a "benign"
redefinition).  If a "``(``" follows immediately (with no intervening white
space), the macro is function-like; its parameter names are accumulated as a
list.  Otherwise, the macro is object-like.  ``proc_define`` then scans the
replacement text and builds the replacement text string in ``macro_buffer``.
It calls ``mdefn_get_token`` to fetch tokens of the body and remember whether
or not any white space preceded each.  White space is standardized,
end-of-token markers are inserted (if in ANSI C or C++ mode), parameter names
are recognized, and ``#`` and ``##`` are handled as described above.

If in pcc mode, the opening and closing quoting characters of character
constants and string literals are scanned as single-character tokens, and the
insides of the quoted strings are tokenized.  This results in recognition (and
later replacement) of parameter names within strings, as ``cpp`` does it.
Invalid tokens are passed textually to the replacement text (this is necessary
always, not just for this case), and skipped white space is kept in its
original form, so the contents of the string will be accurately rendered in the
final expansion of the macro.

After the replacement text is accumulated, it is copied to
dynamically-allocated storage, and the macro definition is completed and
attached to the symbol.  For a redefinition, the new parameter list and
replacement text are first compared textually against the old.  If they match,
the new definition is thrown away.

.. _macro-invocations:

Macro Invocations
-----------------

``macro_invocation`` handles the scanning and replacement of macro invocations.
It is called by ``get_token`` when an identifier that is a macro name is
scanned.

The first thing that ``macro_invocation`` checks is whether or not the macro
name is "inert".  The standard says that when a macro's name is generated in
its own expansion (directly or indirectly), it is forever after protected
against further expansion (it is inert).  This is implemented by the
``assoc_macro`` field in the ``a_source_line_modif`` entry.  A macro invocation
is replaced by the macro's expansion by creating a modification entry that
indicates the textual changes to be made to the source line or to a previous
modification.  The ``assoc_macro`` field records the origin of any given
change, by pointing to the definition of the macro that generated the change.
Given a pointer to the name of a macro at the beginning of a possible macro
invocation, one can therefore determine whether it is in the main source line
or in a modification (or nested in several), and from that one can see whether
the macro name appears as part of its own expansion.  If so, the macro name is
inert, and is left as an identifier.  (Actually, it is replaced by an
``LE_INERT_MACRO`` lexical escape followed by the macro name.  Once a macro
name is preceded by an inert-macro escape, it is considered inert every time it
is scanned, regardless of the context.) In pcc mode, macros are not considered
to be inert, and recursion is checked for by counting the number of nested
invocations of a given macro.

If the macro name is not inert, processing for expansion continues.
``delete_source_from_loc`` is set to point to the beginning of the macro
identifier.  This begins a "hanging delete" which will be closed when the end
of the macro invocation is found.  At that point, the text of the hanging
delete will be replaced by the macro expansion, this replacement being
indicated by a source line modification entry.

If the macro is object-like, no parameters need be scanned.  However, there are
several special cases in object-like macros: For ``__FILE__`` and ``__LINE__``,
the proper expansion string is developed from the current position information
in the input stack; for ``defined``, which is an operator allowed in ``#if``
expressions but handled as a pseudo-macro, the routine
``scan_defined_operator`` is called.  It scans both forms of the ``defined``
operator, and returns ``0`` or ``1``.  Alternatively, if the identifier
``defined`` is not followed by appropriate tokens, it is left as an identifier
(see the discussion of ``check_for_following_parenthesis`` below).

If the macro is function-like, ``check_for_following_parenthesis`` is called to
scan forward looking for a parenthesis that begins the argument list.  If the
next token is not a parenthesis, then the macro name is left as an identifier.
However, this is complicated, because in skipping white space while looking for
a left parenthesis, we may have gone onto a new source line.  We have to delete
the identifier before leaving the old source line, on the assumption that we
are scanning a macro invocation.  If it turns out that the identifier is not
the start of a macro invocation, the identifier must be reinserted into the new
source line.

Note the field ``sequence_id`` in ``a_source_line_modif``; it is an integer
sequence number indicating the sequence in which source line modifications were
added.  This is useful in cases where modifications must be undone (as in
identifier un-deletion when a macro identifier is followed by something other
than a left parenthesis on the same line); one can find all entries with
sequence numbers greater than a certain number, and remove them.

If a left parenthesis *is* found, the argument list is scanned, and this
requires a new data structure.  An entry of type ``a_macro_arg`` contains
the text of a single argument value.  That text exists in two forms -- as
"raw" text (not macro-expanded), and as "expanded" text.  The
``a_macro_arg`` entries are dynamically allocated, and freed when no longer
needed (see ``alloc_macro_arg``, ``free_macro_arg``, and the variable
``avail_macro_args``).  The text of macro arguments is not kept in
``macro_buffer``.

Each macro argument is initially scanned in raw form.  ``arg_get_token`` is
called, with macro expansion turned off, and the characters of each token, with
standardized white space and end-of-token markers, are placed in the array
pointed to by ``raw_text`` in the ``a_macro_arg`` entry (a
dynamically-allocated array, which can be reallocated as needed to expand it).
The scan stops when a comma or right parenthesis is found.  A variable is used
to count the nesting of parentheses, so that non-zero-level commas and right
parentheses will not stop the scan.

After the raw form of the argument has been accumulated, the raw text is
temporarily re-inserted in the source line and is re-scanned with macro
expansion on.  The standard says that this must be done as if this text forms
the rest of the source file (i.e., the macro expansion is done in isolation
from the text following the raw argument value).  This is implemented by having
the source modification entry that re-inserts the raw text have the field
``is_isolated_text`` TRUE.  This will cause ``get_token`` and
``skip_white_space``, upon reaching the ``LE_END_OF_INSERTION`` lexical escape
sequence at the end of the raw text, to return ``tok_end_of_source`` instead of
continuing into the surrounding text.  ``macro_invocation`` scans the entire
raw argument again, saving the text of the tokens scanned in the
``expanded_text`` array in the macro argument entry, and stopping when it gets
``tok_end_of_source``.  If any modifications of the raw text were done because
of macro expansions (this can be determined by sequence id), they are removed.
This restores the raw text to its original state.  In pcc mode, and when the
definition of the macro does not use the parameter in a context that requires
expansion,only the raw form of each argument is needed, so the rescan is not
done.

In cases where the macro argument is the result of a previous macro expansion,
the rescan begins in the text inserted by the macro expansion modifications
rather than in the ``raw_text`` buffer.  That ensures that the inertness test
can be done properly on macro names encountered in the rescanned source.  Once
the rescan reaches the end of the insertions and would continue into the
primary source line, the input stream is switched to the tail of the
``raw_text`` buffer.  (``raw_text`` is used instead of the primary source line
because macro arguments can be continued to subsequent source lines, and once
the next line is read the only copy of the raw argument text is in the
``raw_text`` array; the previous source line is gone.)

This process is repeated for each argument.  After all arguments are scanned
(or immediately for object-like macros), expansion is done.  The length of the
expansion is first determined, and then the textual change is made, by building
up text in ``macro_buffer`` and placing a source line modification to delete
the characters of the macro invocation and insert the characters of the
expansion.

For the special cases of predefined macros, the replacement text is already
available as a simple null-terminated string.  For these cases, determining the
length and building the replacement string are easily done.

For the other, more normal, cases, the length is determined and the expansion
string is built up by interpreting the sections of the replacement text in
order.  For unexpanded uses of parameters, the ``raw_text`` is copied.  For
macro-expanded uses of parameters, the ``expanded_text`` is copied.  For
stringized parameters, ``stringized_arg`` is called; it copies the
``raw_text``, inserting the surrounding quotes and the additional backslashes
to protect quotes and backslashes in the string.

The source line modification is then inserted to perform the macro expansion.

When, in pcc mode, the expansion is for a top-level macro,
``expand_top_level_pcc_macro`` is called to expand any macro calls within the
body of the macro.  A complete string for the expanded version is constructed
in an auxiliary buffer (``aux_buffer_for_pcc_macros``) and then copied back
into ``macro_buffer``, replacing the original expansion and any source line
modifications associated with it.  Getting a fully-expanded string is necessary
to get pcc-like token pasting behavior across internal macro expansion
boundaries.  A variant of this processing is also done in Microsoft mode.

``macro_invocation`` then returns.  It indicates to ``get_token`` whether or
not the inserted text should be rescanned.  If ``macro_invocation`` already
knows what the next token should be, it returns it directly (``*rescan`` =
FALSE), thus saving ``get_token`` the time and trouble of scanning the token
text again.

A utility debug routine ``print_markered_text`` is available for use in
printing out text strings that contain end-of-token markers and/or transitions
into or out of source modifications.

Macro Position Tracking
-----------------------

When ``FULLY_RESOLVED_MACRO_POSITIONS`` is TRUE, the front end keeps track of
the original location of text that is copied into macro expansions -- i.e.,
either in a ``#define`` line or in an argument to a top-level macro invocation.
This "original position" information is reflected in the ``orig_seq`` and
``orig_column`` fields of ``a_source_position``.

The principal data structure used in tracking original position information
through the various manipulations performed during macro definition and
invocation is ``a_macro_text_map``, which defines an extensible array of
``a_macro_text_map_entry`` elements (both defined in ``lexical.h``).  Each of
the repositories of text associated with macro definition and invocation --
macro definitions, the raw and expanded forms of macro arguments, and source
line modifications -- has an associated macro text map.

A macro text map provides an ordered sequence of correspondences between
offsets in the associated text buffer and the original source locations from
which that text was copied.  Each map entry defines a region of the associated
text buffer, beginning at offset ``start_of_region`` and extending up to but
not including the ``start_of_region`` of the next entry.  The text at offset
``start_of_region`` originally came from the location given by
``corresponding_source_pos``, with a one-to-one correspondence of increasing
offsets and source column positions throughout the entry's buffer region.
Because the entries are ordered by buffer offset, the original source position
for a given offset can be efficiently found using a binary search (the
requisite ``bsearch`` predicate is
``compare_macro_text_map_entry_with_offset``, defined in ``lexical.c``).

It will be helpful to trace the overall flow of information through the
definition and invocation of a macro before covering the details of how it is
accomplished.  When a ``#define`` is processed, ``proc_define`` builds a text
map by incrementally recording the source position and offset in the
replacement text for each token of the definition that will be copied to the
macro expansion.  A similar process occurs as each macro argument is copied to
the corresponding raw and expanded text buffers.  During the interpretation of
the replacement text by ``macro_invocation``, as each section of text is copied
from the replacement text or macro argument, the corresponding entries from the
associated macro text map are copied, adjusting the offsets to reflect the
location of the text in the macro expansion (but leaving the original source
positions).  Finally, when ``conv_line_loc_to_source_pos`` is called to compute
a source position for a given character location in the expanded text, the
macro text map in the source line modification is used to determine the
corresponding original source position.

There are two principal patterns for adding entries to a text map.  The simpler
pattern is when a sequence of map entries is copied from one map to another,
adjusting the offsets from being relative to the start of the source buffer to
being relative to the start of the target buffer.  This technique is used to
create the text map for the source line modification containing the macro
expansion by copying portions of the text maps for the macro definition
replacement text and macro arguments.  The function that performs this
operation is ``clone_macro_text_map_entries``, defined in ``macro.c``.

The other pattern occurs when a text buffer is built incrementally by fetching
one token at a time and adding its text to a buffer.  In this case, the map
entries are built to reflect either the source position of the token, if the
token is in source text, or a copy of the corresponding text map entry, if the
token comes from a source line modification (e.g., a macro argument in a macro
invocation appearing in the expansion of another macro invocation).  This
technique is used to create the replacement text in a macro definition and for
both the raw and expanded text of macro arguments.  It is also used when
processing a top-level macro invocation in pcc and Microsoft modes.

Facilitating this process are the struct ``a_text_map_position_tracker`` and
the functions ``init_text_map_position_tracker``,
``add_token_to_macro_text_map``, and ``terminate_macro_text_map`` (all defined
in ``macro.c``).  As each token is added to the text buffer,
``add_token_to_macro_text_map`` is called to determine whether a new entry is
required in the map for the new token: if the token is from the same source
line or source line modification as the preceding token and its offset within
the current buffer region is the same as the column offset from the region's
corresponding source position, then no new map entry is needed.  If a new map
entry is required, either the appropriate map entries from the previous token's
source line modification are copied, using ``clone_macro_text_map_entries``, or
(for tokens from the current source line) a new entry is added covering the
appropriate region of the current source line for the preceding token(s).

One complication in this processing occurs when ``macro_buffer`` is
reallocated, as described earlier.  Because ``a_text_map_position_tracker``
objects in use at the time ``macro_buffer`` is reallocated might contain the
offset of text in a source line modification's inserted text that occurs after
a portion of deleted text, and because that deleted text might be compacted as
part of the reallocation process, it is necessary to keep track of all active
position tracker objects so their current offset values can be adjusted if
necessary.

This is done using the variable ``active_text_map_position_trackers`` (in
``macro.c``), which is the top of a stack of all trackers currently in use, and
the field ``num_active_position_trackers`` in ``a_source_line_modif``.
Whenever a token scan under control of a text map position tracker enters the
inserted text of a source line modification, its
``num_active_position_trackers`` is incremented, and it is decremented when the
scan leaves that source line modification.

Whenever ``macro_buffer`` is reallocated, all the entries in each source line
modification's text map are examined and each offset adjusted, if necessary, to
compensate for the removal of deleted text.  Furthermore, if the modification's
``num_active_position_trackers`` field is greater than zero, all the trackers
in the ``active_text_map_position_trackers`` stack are checked for references
to that source line modification and their offsets adjusted as needed.

The text maps for macro definitions and the raw and expanded text of macro
arguments are completely self-contained: each has its own array of text map
entries and is associated with the containing data structure for the duration
of the compilation.  (The array of entries for a macro definition is of fixed
size and is allocated in front-end memory, so that it will be stored in a
precompiled header, while the arrays for macro arguments are extensible.) A
different scheme is used for the text maps in source line modifications,
however.  The reason for this decision is that the text map for a source line
modification may well be very large, with thousands or tens of thousands of
entries, while source line modifications themselves are ephemeral, generally no
longer needed once processing of the current logical source line has been
completed.  The same reasoning led to the use of ``macro_buffer`` for the
inserted text of source line modification, so the memory management for source
line modification text maps is patterned after that of ``macro_buffer``.  Just
as each modification's inserted text is simply a subrange of the text in
``macro_buffer``, the entries created during macro expansion are built in a
single map, called ``macro_text_map`` and defined in ``macro.c``, and each
modification's text map simply refers to a subrange of them.  Initialization
and truncation of ``macro_text_map`` is done in parallel with the corresponding
operations on ``macro_buffer``.

Macro Invocation Tree
---------------------

Maintaining the macro invocation tree is selected by setting
``MACRO_INVOCATION_TREE_IN_IL`` to TRUE.  Each macro invocation is recorded in
an object of type ``a_macro_invocation_record`` (defined in ``il_def.h``),
giving the parent invocation, if any; the macro invoked; and its starting
position (as well as its ending position, if ``EXTRA_SOURCE_POSITIONS_IN_IL``
is set to TRUE).  (The "parent invocation" refers to the macro invocation in
whose argument list or expansion this macro invocation occurred.  For macro
invocations occurring in program text and not in a macro argument list,
``parent_macro_index`` will have the value ``NO_PARENT_MACRO_INVOCATION``.)

Each time ``macro_invocation`` is called to expand a macro, it calls
``register_macro_invocation`` to add an entry to the list and obtain the index
of the newly-added entry.  This index and the macro invocation stack depth at
that point are stored into the source line modification created to hold the
macro's expanded text.  These values are used by recursive calls of
``macro_invocation`` (to expand macro arguments) and by calls resulting from
rescanning expanded text to maintain the chain of parent invocations when the
macro name comes from a source line modification rather than directly from
source text.

Because variable-length data is awkward to represent in the IL (and extensible
arrays are not possible in memory regions), the macro invocation records are
grouped into fixed-size blocks (``a_macro_invocation_record_block``, defined in
``il_def.h``).  In the IL, these blocks are organized into a binary tree to
facilitate relatively-efficient random access by index.  During front-end
processing, however, they are kept in a doubly-linked list: apart from
diagnostic output, which need not be terribly efficient, there is no need for
random access to the invocation records (and even diagnostic output will most
often refer to records near the end of the list, so long linear searches are
uncommon).  It is more efficient to create the binary tree form once, after the
total number of records is known, than to balance the tree repeatedly while it
is being created.

The ``macro_invocation_record_block``\ s are reorganized into a binary tree and
the relevant fields of ``il_header`` are set at the end of the translation unit
by a call to ``copy_macro_invocation_tree_to_il`` (defined in ``macro.c``).

An additional effect of setting ``MACRO_INVOCATION_TREE_IN_IL`` is that
``a_source_position`` is extended to include the ``macro_context`` field.  This
field reflects the macro invocation in whose expansion the source position
resides, i.e., it is logically simply a copy of the ``invocation_record`` field
of the containing source line modification.  However, the special treatment
given to top-level macros in pcc and Microsoft mode, in which the expansion of
the macro is rescanned for expansion and the result is copied back into the
source line modification corresponding to the top-level macro invocation, means
that the subsidiary source line modifications are no longer available at the
time source positions are being computed.

To avoid this problem, the macro invocation index that will be stored by
``macro_invocation`` into source line modifications is also kept in the
``macro_context`` field of each text map entry created for that invocation.
This information is preserved when the text maps for the subsidiary source line
modifications are cloned into the top-level modification by
``expand_top_level_pcc_macro``.  This additional information allows the
``macro_context`` field in source positions to be set in the same way as the
original position information, by looking up the offset in the text map of the
top-level modification.  (This is the reason that it is not permitted to set
``MACRO_INVOCATION_TREE_IN_IL`` to TRUE without also setting
``FULLY_RESOLVED_MACRO_POSITIONS`` to TRUE.)

Unicode Version-Dependent Features
==================================

The front end has three distinct features that depend on the definitions
and classifications of characters in the Unicode standard and thus might
need to be updated when new versions of Unicode are released.  The following
subsections discuss each of these features and what is involved in a potential
update.

Classification of Identifier Characters
---------------------------------------

The front end must, of course, determine whether a given character can
appear in a C/C++ identifier in order to find the start and end of
identifier tokens during lexical analysis. There is also a distinction
between characters that can begin an identifier and those that can appear
only after the initial character; for example, ``'0'`` cannot be the first
character of an identifier but it is valid in the succeeding positions.

For single-byte character sets, this classification is done using boolean
arrays indexed by the character value; see the declarations of
``is_id_char``, ``is_id_char_no_mbc``, and ``char_ends_id`` in
``lexical.h`` and their initialization in ``lexical_one_time_init`` in
``lexical.c`` for details.

The situation is more complicated for multi-byte characters, whether
appearing directly in the source or via a *universal-character-name*. For
C++23 (and, because the relevant paper was adopted as a defect report, for
previous versions as well) and C23, the determination is based on the
Unicode ``XID_Start`` and ``XID_Continue`` Derived Properties as specified
in `www.unicode.org/Public/UCD/latest/ucd/DerivedCoreProperties.txt
<https://www.unicode.org/Public/UCD/latest/ucd/DerivedCoreProperties.txt>`__.
(Specific versions of this file can be found by replacing ``UCD/latest``
with the version number, e.g., ``15.1.0``, in the URL.) Earlier versions of
the C and C++ standards used different methods to specify these
classifications, however, and the front end continues to support those
variants for backward compatibility.

The mechanism for achieving compatibility with multiple classification
schemes is the ``UCN_table`` array, defined in ``lexical.c``. The elements
of the array are objects of the following struct:

.. code:: c++

  typedef struct a_UCN_range {
    unsigned long start;  /* The first character in the range. */
    unsigned long end;    /* The last character in the range. */
    a_byte        dialects;
                          /* A bit set indicating the dialects for which this
                             range of characters is accepted.  See below for
                             the encoding masks. */
    a_byte        non_initial_dialects;
                          /* A bit set indicating the dialects for which this
                             range of characters is not permitted as the
                             first character of an identifier.  This uses the
                             same encoding as dialects. */
  } a_UCN_range;

Characters tend to occur in groups, so to reduce the storage requirements
of the array, each element describes a contiguous range of characters, all
of which share the same treatment in all of the supported classification
schemes. The elements of the array are sorted in strictly-ascending order
of code point so that the array can be efficiently searched using a binary
search. The ``dialects`` and ``non_initial_dialects`` members are bit sets
with each bit representing a different language dialect, specifying how the
characters in the specified range are classified in the various
dialects. The C++23/C23 dialect is represented by the ``UAX44`` bit (so
named because the ``XID_Start`` and ``XID_Continue`` properties are
described in Unicode Annex #44).

From the perspective of updating this table when new versions of Unicode
are published, the principal requirement of this table is that all members
of a given character range must be classified the same way in all of the
supported dialects. A change in the classification of a character in a new
Unicode version thus typically means that an existing range in the table
must be split, so that any characters before and/or after the character of
interest can maintain their previous values while that character's
``UAX44`` bits reflect the new Unicode classification (maintaining the
previous values for all the other flags). As a specific example, the
``DerivedCoreProperties.txt`` file for Unicode version 15.1.0 contained
(among others) the following changes from version 15.0.0:

.. code:: text

  +2EBF0..2EE5D  ; XID_Start # Lo [622] CJK UNIFIED IDEOGRAPH-2EBF0..CJK UNIFIED IDEOGRAPH-2EE5D
  . . .
  +2EBF0..2EE5D  ; XID_Continue # Lo [622] CJK UNIFIED IDEOGRAPH-2EBF0..CJK UNIFIED IDEOGRAPH-2EE5D

These changes indicate that the characters in the indicated range can now
appear both within identifiers and as the first character of an
identifier. These code points were previously contained within the range of
the following element of the ``UCN_table`` array:

.. code:: c++

  { 0x2ebe1, 0x2f7ff,   _   |  _  | CPP11 |   _  ,   _   |   _   |   _   },

As a result, the range needed to be split in order to preserve the
classification of the characters before and after the new 15.1.0 range,
resulting in the following three elements to replace the existing one:

.. code:: c++


  { 0x2ebe1, 0x2ebef,   _   |  _  | CPP11 |   _  ,   _   |   _   |   _   },
  { 0x2ebf0, 0x2ee5d,   _   |  _  | CPP11 | UAX44,   _   |   _   |   _   },
  { 0x2ee5e, 0x2f7ff,   _   |  _  | CPP11 |   _  ,   _   |   _   |   _   },

Note both the addition of the ``UAX44`` flag in the initializer for
``dialects`` and its absence in the initializer for
``non_initial_dialects``, since the latter specifies dialects for which the
characters are *not* permitted to start an identifier.

Detection of Source Code Unicode Vulnerabilities
------------------------------------------------

One potential avenue of source-level exploits relies on the fact that a
number of distinct Unicode characters have similar or identical glyphs. For
example, the Latin, Greek, and Cyrillic alphabets each have a character
whose visual appearance is "A", even though the code points for those
characters are different. This permits the creation of programs with
vulnerabilities that cannot be detected by even a careful code review (by
hijacking overloaded function calls, for example).

Unicode provides a table,
`www.unicode.org/Public/security/latest/confusables.txt
<https://www.unicode.org/Public/security/latest/confusables.txt>`__, that
enumerates such potentially-confused characters (replace "latest" with the
version number to access the specification for a specific version). This
table is described in
`www.unicode.org/reports/tr39/ <https://www.unicode.org/reports/tr39/>`__.

The front end incorporates this data as the array ``confusable_map``,
defined in ``lexical.h``, which is a lightly-massaged version of the
contents of ``confusables.txt``. Each element of the array is an object of
the following struct:

.. code:: c++

  struct a_confusable_map_elem {
    int	src_char;         /* The numeric value of a Unicode code point whose
                             glyph could be confused with that of another
                             code point. */
    int	prototype[MAX_PROTOTYPE_LENGTH];
                          /* The numeric values of the Unicode code points of
                             the prototype character(s) for src_char, as
                             defined in unicode.org/reports/tr39, "Unicode
                             Security Mechanisms", section 4. */
  };  /* a_confusable_map_elem */

The elements are in strictly-ascending order of ``src_char``, to facilitate
efficient searching using a binary search.

Although the entries in ``confusables.txt`` are not in the same order as
the elements of ``confusable_map``, that does not present a significant
issue for updating the front end for new Unicode versions, as the mapping
from the Unicode data to the corresponding array initializer is
obvious. For example, a representative line of the version 15.1.0 file is:

.. code:: text

  2238 ;	002D 0307 ;	MA	#* ( ∸ → -̇ ) DOT MINUS → HYPHEN-MINUS, COMBINING DOT ABOVE	#

and the corresponding initializer element is:

.. code:: c++

  { 0x02238, { 0x002D, 0x0307 } },

Thus, a simple ``diff`` of two versions of the ``confusables.txt`` file
provides sufficient information for any required updates.

Such updates are expected to be infrequent, in any event, as the base data
is not likely to change very much over time. If desired, the
``edg-make-confusables-data`` script can be used with a given version of
``confusables.txt`` to create the entire initializer for
``confusable_map``.

Named Unicode Escapes
---------------------

C++23 introduced the capability of incorporating a given Unicode character
into the source text by specifying its full name, using the syntax ``\N{``\
*character-name*\ ``}``. This feature requires the front end to be able to
distinguish valid *character-name*\ s from invalid ones and to determine the
code point associated with each valid name.

Because Unicode defines tens of thousands of characters, with the average
name being over thirty characters in length, this task is necessarily
data-intensive. A naive straightforward approach, e.g., a sorted array of
character names and code points searched using a binary search, would
require around 1.5 megabytes of data and up to sixteen probes to match a
given character name, so it would be fairly inefficient in both data and
execution time.

The approach chosen for the front end implementation of this feature uses a
fairly straightforward finite state machine (FSM), with each state
representing the initial substring of characters in one or more character
names and each transition designating the state that would result for a
specific character following that initial substring in a valid Unicode
character name. There are a few simple optimizations to reduce the size of
the data containing the FSM; sequences of single-transition states are
compressed into a single state that matches a sequence of characters and
has a single transition on the final one, and the algorithmically-named
ranges for CJK and Tangut ideographs are also condensed into single states
matching the common prefix and specifying their respective ranges of code
points. Incorporating these optimizations, the storage required for the
state machine for the names in Unicode 15.1.0 is about 510 kilobytes, much
less than the naive implementation, and its performance is also much
better.

The data for the Unicode character names and code points comes from two
files: `www.unicode.org/Public/UCD/latest/ucd/UnicodeData.txt
<https://www.unicode.org/Public/UCD/latest/ucd/UnicodeData.txt>`__ and
`www.unicode.org/Public/UCD/latest/ucd/NameAliases.txt
<https://www.unicode.org/Public/UCD/latest/ucd/NameAliases.txt>`__.  (The
files for specific versions can be found by substituting the version
number, e.g., ``15.1.0``, for ``UCD/latest`` in the URLs.)

The data for the state machine in the front end is contained in the array
``unicode_name_fsm``, defined in the file ``unicode_name_fsm.c``. Because
this table cannot be edited by hand, there is a utility that produces that
file in its entirety, with no manual editing required. To run the utility,
download the two files above from the ``unicode.org`` web site and issue
the following command:

  ``process_unicode_names UnicodeData.txt NameAliases.txt`` *output-file version date*

where *output-file* is any file name (it must be copied to
``unicode_name_fsm.c`` if that file is not updated *in situ*); *version* is
the Unicode version number (e.g., ``15.1.0``); and *date* is the release
date of the Unicode version (e.g., ``2023-08-28``). (The latter two
command-line arguments are incorporated into the comments in the generated
source file.)

The source for the utility is found in the
``dev_tools/cpp_tools/process_unicode_names`` subdirectory. Any changes
desired in the contents of ``unicode_name_fsm.c`` (such as updating the
copyright date) should be edited into the utility source in the
initializer for the ``header_text`` variable, since any manual edits in
``unicode_name_fsm.c`` will be overwritten by the next run of the utility.

.. [#f1] Trigraphs and line splices are not undone and so do not appear in the
         output.
.. [#f2] As mentioned previously, the actual newline character is replaced by
         a lexical escape sequence beginning with ``LE_ESCAPE`` (zero) so that
         the newline character can be freed up for use as ``ATTENTION_MARKER``.
.. [#f3] See :ref:`recognizing-generalized-identifiers` for more
         information regarding vacuous destructors.
.. [#f4] A name is "looked up" by calling the appropriate symbol table lookup
         routine, which updates the ``specific_symbol`` field in the symbol
         locator.
.. [#f5] ``A`` is equivalent to ``A<T>`` in the scope of a template class
         instance.
.. [#f6] Which is used in looking up identifiers that might be types or might
         be part of a declarator.
