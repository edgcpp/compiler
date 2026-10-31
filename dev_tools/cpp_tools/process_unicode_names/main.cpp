/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*
Utility program that creates a C/C++ source file containing a finite state
machine for recognizing a Unicode character name and determining its
associated code point.  The input data is taken from two files.  The first
is assumed to be in the form of the file

  https://www.unicode.org/Public/18.0.0/ucd/UnicodeData.txt

where each line consists of

  <code point>;<name or category>;...

"code point" is four to six hexadecimal digits giving the numeric value of
the associated character.  "name or category" is either the name of a
Unicode character (consisting solely of the characters 0-9, A-Z, hyphen,
and the space character) or a category name enclosed in angle brackets,
such as "<control>".  The names and code points of the lines in the first
category are added to the state machine.  Some pairs of lines in the second
category define a range of characters, e.g.,

  4E00;<CJK Ideograph, First>;...
  9FFF;<CJK Ideograph, Last>;...

Characters in the ranges for the CJK and Tangut ideographs, Jurchen and
Small Seal characters, along with the Hangul syllables, are also added to
the machine.

The second file is assumed to be in the form of the file

  https://www.unicode.org/Public/18.0.0/ucd/NameAliases.txt

which consists of empty lines, comment lines beginning with '#', and data
lines of the form

  <code point>;<name>;<category>

where "code point" and "name" are as in the first file.  The names and code
points from data lines whose category is "correction", "control", or
"alternate" are added to the state machine; all other lines are ignored.

The state machine is initially created as a collection of linked a_state,
a_transition, and a_code_point_range objects, as defined below.  Once the
state machine is complete, it is flatterned into a byte array that will be
used as data by the front end to process named Unicode escapes.  See the
comments inserted into the generated output file for a description of the
format and contents of the flattened state machine.
*/

/*
Files and error handling.
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>

void report_error_and_exit(const char *message, const char *plugin)
/*
Write the specified message to stderr and exit with an error return value.
*/
{
  fprintf(stderr, message, plugin);
  exit(2);
}  /* report_error_and_exit */


/*
Definition of an element of a linked list of code point ranges.
*/
struct a_code_point_range {
  a_code_point_range
		*next;	/* The next range object, or null for the last
			   range in the list. */
  unsigned	first;	/* The first code point in the range. */
  unsigned	last;	/* The last code point in the range. */
  static unsigned
		count;	/* Total number of range objects created. */
  a_code_point_range(unsigned range_start, unsigned range_end);
};  /* a_code_point_range */
unsigned a_code_point_range::count = 0;


a_code_point_range::a_code_point_range(unsigned           range_start,
                                       unsigned           range_end)
  : next(nullptr), first(range_start), last(range_end) {
  ++count;
}  /* a_code_point_range::a_code_point_range */


/*
Definition of a state object.  There are three kinds of states in the state
machine: terminal states, range states, and intermediate states.  Terminal
states have a single terminal transition, i.e., a transition for the
character '}' that gives the code point for the (single) character whose
name is matched by the current state.  A range state is like a terminal
state except that it has no transitions but directly represents multiple
code points (all the code points in the associated ranges, with names
generated algorithmically from the value of the corresponding code point).
Intermediate states have one or more transitions that lead to other states
when the transition's character is matched.
*/
struct a_transition;
struct a_state {
  unsigned	num_transitions;
			/* The number of transitions in this state, 0 for
			   range states. */
  unsigned	num_ranges;
			/* The number of code ranges in this state, 0 for
			   states with transitions. */
  a_transition	*transitions;
			/* Linked list of transitions out of this state;
			   null for range states. */
  a_code_point_range
		*code_point_ranges;
			/* Linked list of code ranges for this state; null
			   for states with transitions. */
  static unsigned
		count;	/* Total number of state objects created. */
  static unsigned
		terminal_states;
			/* Number of states that consist only of a single,
			   terminal transition and thus are eliminated from
			   the flattened form of the machine. */
  static unsigned
		suppressed_single_trans_states;
			/* The number of single-transition states that are
			   eliminated by use of multi-character states. */
  a_state();
  void add_range(unsigned first, unsigned last);
};  /* a_transition */
unsigned a_state::count = 0;
unsigned a_state::terminal_states = 0;
unsigned a_state::suppressed_single_trans_states = 0;


a_state::a_state()
/*
Constructor for a state object.
*/
  : num_transitions(0), num_ranges(0), transitions(nullptr),
    code_point_ranges(nullptr) {
  ++count;
}  /* a_state::a_state */


void a_state::add_range(unsigned first, unsigned last)
/*
Add a range object for the specified character range to the list of ranges.
*/
{
  if (num_transitions != 0) {
    report_error_and_exit("Adding range to a state with transitions.", "");
  }  /* if */
  a_code_point_range *range = new a_code_point_range(first, last);
  range->next = code_point_ranges;
  code_point_ranges = range;
  ++num_ranges;
}  /* a_state::add_range */


/*
Definition of a transition object.
*/
struct a_transition {
  char		name_char;
			/* The character for which this transition is
			   selected. */
  unsigned	code_point;
			/* If name_char is '}', this transition is
			   terminal; state will be null and this is the
			   code point of the associated Unicode
			   character. */
  a_state	*state;	/* If name_char is not '}', this is the state that
			   will become current when name_char is seen in
			   the current state; in this case, code_point is
			   not meaningful. */
  a_transition	*next;	/* The next transition in the list of transitions
			   for the current state, null if this is the last
			   transition in the list. */
  static unsigned
		count;	/* Total number of transition objects created. */
  a_transition(a_state *state, char name_ch);
  a_transition(a_state *state, unsigned code_pt);
};
unsigned a_transition::count = 0;


a_transition::a_transition(a_state *from_state, char name_ch)
/*
Constructor for an intermediate a_transition object.  Record the specified
name character, create a new state as the target state, and update the count
of transitions in the designated state.
*/
  : name_char(name_ch), code_point(0), state(new a_state), next(nullptr) {
  ++count;
  if (from_state->num_ranges != 0) {
    report_error_and_exit(
                 "Adding intermediate transition to a state with ranges.", "");
  }  /* if */
  ++from_state->num_transitions;
}  /* a_transition::a_transition */


a_transition::a_transition(a_state *from_state, unsigned code_pt)
/*
Constructor for a terminal a_transition object.  Set the name character to
'}' to identify this as a terminal transition, record the specified code
point, and update the count of transitions in the designated state.
*/
  : name_char('}'), code_point(code_pt), state(nullptr), next(nullptr) {
  ++count;
  if (from_state->num_ranges != 0) {
    report_error_and_exit(
                     "Adding terminal transition to a state with ranges.", "");
  }  /* if */
  ++from_state->num_transitions;
}  /* a_transition::a_transition */


/*
The initial state of the state machine.
*/
a_state *initial_state = new a_state;


void add_name_and_code_points(unsigned   code_pt,
                              unsigned   end_code_pt,
                              const char *name,
                              const char *end_of_input)
/*
Add states and transitions to the state machine associating the given name
with the given code point or range of code points.  name points to the
first character of a Unicode name or prefix.  If the name is terminated by
a ';' character, the name represents a single Unicode character;
end_code_pt must be 0, and a terminal state will be added.  If the name is
terminated by a null character, the name is a prefix; end_code_pt must be
greater than code_pt and a range state designating the range from code_pt
to end_code_pt will be added.  If neither a ';' nor a null character is
found before end_of_input, an error is reported.
*/
{
  a_state *state = initial_state;
  for (;;) {
    a_transition *transition;
    a_transition *last_transition = nullptr;
    for (transition = state->transitions; transition != nullptr;
         last_transition = transition, transition = transition->next) {
      if (transition->name_char == *name) {
        /* This is an existing transition for this state and name_char -
           i.e., the name up to this point shares a common prefix with a
           previously-seen name.  Follow the transition and continue. */
        state = transition->state;
        ++name;
        break;
      }  /* if */
    }  /* for */
    if (transition == nullptr) {
      /* This is the first time this name_char has been seen in this state.
         Add a new transition in the current state. */
      a_transition **trans_link;
      if (last_transition == nullptr) {
        /* This is the first transition in the current state. */
        trans_link = &state->transitions;
      } else {
        /* Add the new transition at the end of the list. */
        trans_link = &last_transition->next;
      }  /* if */
      if (*name == ';') {
        /* This is the end of the name of a single Unicode character.  Add
           a terminal transition giving the specified code point, and we're
           done. */
        if (end_code_pt != 0) {
          report_error_and_exit("Invalid code point entry.", "");
        }  /* if */
        *trans_link = new a_transition(state, code_pt);
        break;
      } else if (*name == '\0') {
        /* This is the end of a prefix for a range of Unicode characters.
           Add the specified range to the current state. */
        if (end_code_pt < code_pt) {
          report_error_and_exit("Invalid range specification.", "");
        }  /* if */
        state->add_range(code_pt, end_code_pt);
        break;
      } else {
        /* This will be an intermediate transition.  Add it to the current
           state and continue with the newly-created state and the next
           character of the name. */
        *trans_link = new a_transition(state, *name);
        ++name;
        state = (*trans_link)->state;
      }  /* if */
    }  /* if*/
    if (name >= end_of_input) {
      report_error_and_exit("Character name not terminated by ';'\n", "");
    }  /* if */
  }  /* for */
}  /* add_name_and_code_points */


inline unsigned value_of_hex_digit(char ch)
/*
If ch is a hexadecimal character (0-9, A-F, a-f), return the corresponding
numeric value; otherwise, report an error and exit.
*/
{
  unsigned result = 0;

  if (ch >= 'a' && ch <= 'f') {
    result = 10 + ch - 'a';
  } else if (ch >= 'A' && ch <= 'F') {
    result = 10 + ch - 'A';
  } else if (ch >= '0' && ch <= '9') {
    result = ch - '0';
  } else {
    char str[2] = { ch, '\0' };
    report_error_and_exit("Non-hexadecimal digit %s in number\n", str);
  }  /* if */
  return result;
}  /* value_of_hex_digit */


unsigned get_hex_value(char **cp, char *buffer_end)
/*
Read the string of hexadecimal digits beginning at *cp and ending with a
semicolon and return its numeric value, updating *cp to point to the
terminating semicolon.  If the string of hexadecimal digits is not
terminated before buffer_end or if a non-hexadecimal character other than
semicolon is encountered, report an error and exit.
*/
{
  char     *p;
  unsigned result = 0;

  for (p = *cp; p < buffer_end && *p != ';'; ++p) {
    result = (result << 4) + value_of_hex_digit(*p);
  }  /* for */
  if (p >= buffer_end) {
    report_error_and_exit("Invalid format in data file.\n", "");
  }  /* if */
  *cp = p;
  return result;
}  /* get_hex_value */


/*
Definitions related to the algorithmic generation of character names for
the Hangul Syllable block.  The data and algorithm reflect those of the
Hangul Character Name Generation pseudocode found in section 3.12 of
www.unicode.org/versions/Unicode18.0.0/core-spec/chapter-3/.
*/

#define Hangul_LCount 19	// Number of leading Jamo consonants
#define Hangul_VCount 21	// Number of Jamo vowels
#define Hangul_TCount 28	// Number of trailing Jamo consonants
#define Hangul_SCount (Hangul_LCount * Hangul_VCount * Hangul_TCount)
				// Total number of Hangul syllable characters

/*
Jamo leading consonant names.
*/
const char *hangul_l_name[Hangul_LCount] = {
  "G",   "GG",  "N",   "D",   "DD",  "R",   "M",   "B",
  "BB",  "S",   "SS",  "",    "J",   "JJ",  "C",   "K",
  "T",   "P",   "H"
};  /* hangul_l_name */

/*
Jamo vowel names.
*/
const char *hangul_v_name[Hangul_VCount] = {
  "A",   "AE",  "YA",  "YAE", "EO",  "E",   "YEO", "YE",
  "O",   "WA",  "WAE", "OE",  "YO",  "U",   "WEO", "WE",
  "WI",  "YU",  "EU",  "YI",  "I"
};  /* hangul_v_name */

/*
Jamo trailing consonant names.
*/
const char *hangul_t_name[Hangul_TCount] = {
  "",    "G",   "GG",  "GS",  "N",   "NJ",  "NH",  "D",
  "L",   "LG",  "LM",  "LB",  "LS",  "LT",  "LP",  "LH",
  "M",   "B",   "BS",  "S",   "SS",  "NG",  "J",   "C",
  "K",   "T",   "P",   "H"
};  /* hangul_t_name */

/*
Common prefix for all generated Hangul syllable names.
*/
#define Hangul_name_prefix "HANGUL SYLLABLE "

/*
Offset of the first character of the generated portion of the syllable
character name.
*/
#define Hangul_syllable_name_start (sizeof(Hangul_name_prefix) - 1)

/*
Buffer for generated character names.  The size is the length of the prefix,
plus two characters for the leading consonant name and three characters each
for the vowel and the trailing consonant, plus two characters for the
terminating semicolon and null character.
*/
#define name_buffer_len Hangul_syllable_name_start + 10
char character_name[name_buffer_len] = Hangul_name_prefix;


inline void add_to_character_name(const char *str,
                                  int        &offset)
/*
Add the characters of the null-terminated character string str to
hangul_syllable_character_name beginning at offset and update offset to
designate the position following the copied characters.
*/
{
  while (*str != '\0') {
    character_name[offset++] = *str++;
  }  /* while */
}  /* add_to_character_name */


void add_hangul_syllable_characters(unsigned first_char,
                                    unsigned last_char)
/*
Add the Hangul syllable characters with algorithmically-generated names
for the code points first_char through last_char.
*/
{
  if ((last_char - first_char + 1) != Hangul_SCount) {
    report_error_and_exit("Hangul syllable block size mismatch.", "");
  }  /* if */
  for (int l = 0; l < Hangul_LCount; ++l) {
    int offset = Hangul_syllable_name_start;
    add_to_character_name(hangul_l_name[l], offset);
    int start_of_vowel = offset;
    for (int v = 0; v < Hangul_VCount; ++v) {
      offset = start_of_vowel;
      add_to_character_name(hangul_v_name[v], offset);
      int start_of_trailing_consonant = offset;
      for (int t = 0; t < Hangul_TCount; ++t) {
        offset = start_of_trailing_consonant;
        add_to_character_name(hangul_t_name[t], offset);
        add_to_character_name(";", offset);
        character_name[offset] = '\0';
        add_name_and_code_points(first_char++, 0, character_name,
                                character_name + name_buffer_len);
      }  /* for */
    }  /* for */
  }  /* for */
}  /* add_hangul_syllable_characters */


void process_data_file(FILE *data_file)
/*
Add states and transitions for the contents of a file, open for input on
data_file, that is in the form of UnicodeData.txt.  The file must consist
solely of data lines, no blank lines and no comments.
*/
{
  char buffer[240];
  char *buffer_end = buffer + sizeof(buffer);
  char *p;

  while (fgets(buffer, sizeof(buffer), data_file) != NULL) {
    p = buffer;
    unsigned code_point = get_hex_value(&p, buffer_end);
    if (p[1] == '<') {
      /* See if this is the start of a range, which might require special
         handling.  If not, ignore the line. */
      if (strstr(p + 2, "First>;") != nullptr) {
        /* This is the start of a range.  The next line should be the
           ending code point of the range. */
        if (fgets(buffer, sizeof(buffer), data_file) == NULL ||
            strstr(buffer, "Last>;") == nullptr) {
          report_error_and_exit("End of range not found.", "");
        }  /* if */
        p = buffer;
        unsigned ending_point = get_hex_value(&p, buffer_end);
        if (p[1] != '<' || ending_point < code_point) {
          report_error_and_exit("Invalid specification of end of range.", "");
        }  /*  if */
        if (p[2] == 'H' && strncmp(p + 2, "Hangul Syllable",
                                   sizeof("Hangul Syllable") - 1) == 0) {
          add_hangul_syllable_characters(code_point, ending_point);
        } else if (p[2] == 'C' && strncmp(p + 2, "CJK Ideograph",
                                          sizeof("CJK Ideograph") - 1) == 0) {
          static const char CJK_prefix[] = "CJK UNIFIED IDEOGRAPH-";
          add_name_and_code_points(code_point, ending_point, CJK_prefix,
                                   CJK_prefix + sizeof(CJK_prefix));
        } else if (p[2] == 'T' &&
                   strncmp(p + 2, "Tangut Ideograph",
                           sizeof("Tangut Ideograph") - 1) == 0) {
          static const char Tangut_prefix[] = "TANGUT IDEOGRAPH-";
          add_name_and_code_points(code_point, ending_point, Tangut_prefix,
                                   Tangut_prefix + sizeof(Tangut_prefix));
        } else if (p[2] == 'J' &&
                   strncmp(p + 2, "Jurchen Character",
                           sizeof("Jurchen Character") - 1) == 0) {
          static const char Jurchen_prefix[] = "JURCHEN CHARACTER-";
          add_name_and_code_points(code_point, ending_point, Jurchen_prefix,
                                   Jurchen_prefix + sizeof(Jurchen_prefix));
        } else if (p[2] == 'S' &&
                   strncmp(p + 2, "Seal Character",
                           sizeof("Seal Character") - 1) == 0) {
          static const char Seal_prefix[] = "SMALL SEAL CHARACTER-";
          add_name_and_code_points(code_point, ending_point, Seal_prefix,
                                   Seal_prefix + sizeof(Seal_prefix));
        }  /* if */
      }  /* if */
    } else {
      /* An ordinary character name and code point. */
      add_name_and_code_points(code_point, 0, p + 1, buffer_end);
    }  /* if */
  }  /* while */
  if (!feof(data_file)) {
    report_error_and_exit("Error reading data file.\n", "");
  }  /* if */
}  /* process_data_file */


void process_alias_file(FILE *alias_file)
/*
Add states and transitions for the contents of a file, open for input on
alias_file, that is in the form of NameAlias.txt.  Empty lines, comment
lines beginning with '#', and lines in which the category is not one of
"correction", "control", or "alternate" are ignored.
*/
{
  char buffer[160];
  char *buffer_end = buffer + sizeof(buffer);
  char *p;

  while(fgets(buffer, sizeof(buffer), alias_file) != NULL) {
    p = buffer;
    if (*buffer != '\n' && *buffer != '#') {
      unsigned code_point = get_hex_value(&p, buffer_end);
      if (p >= buffer_end) {
        report_error_and_exit("Invalid format in alias file.\n", "");
      }  /* if */
      char *name_start = p + 1;
      /* Find the ';' that terminates the name and check the category. */
      for (p = name_start + 1; p < buffer_end && *p != ';'; ++p) {}
      if (p >= buffer_end) {
        report_error_and_exit("Invalid format in alias file.\n", "");
      }  /* if */
      if (strncmp(p + 1, "control", sizeof("control") - 1) == 0 ||
          strncmp(p + 1, "correction", sizeof("correction") - 1) == 0 ||
          strncmp(p + 1, "alternate", sizeof("alternate") - 1) == 0) {
        add_name_and_code_points(code_point, 0, name_start, p + 1);
      }  /* if */
    }  /* if */
  }  /* while */
  if (!feof(alias_file)) {
    report_error_and_exit("Error reading alias file.\n", "");
  }  /* if */
}  /* process_alias_file */


/*
Declarations associated with the flattened form of the state machine.
*/

char		*buffer;
			/* Storage in which to place the flattened version
			   of the state machine.  It will be allocated with
			   enough space to contain all the states,
			   transitions, and ranges that have been created,
			   but the actual amount of storage used will be
			   less, due to suppression of states consisting
			   only of a terminal transition and sequences of
			   single-transition states. */
unsigned	next_offset = 0;
			/* The offset within the buffer storage at which
			   the next state and its transitions will be
			   located. */


inline void encode_number(char     *target,
                          unsigned value)
/*
Encode the given value (which must fit in 24 bits) into the three bytes
beginning with target, most-significant byte first.
*/
{
  unsigned orig_value = value;

  target[2] = value & 0xff;
  value >>= 8;
  target[1] = value & 0xff;
  value >>= 8;
  target[0] = value & 0xff;
  if ((value >>= 8) != 0) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%x", orig_value);
    report_error_and_exit("Value too large: %s\n", buffer);
  }  /* if */
}  /* encode_number */


void encode_state(a_state *state)
/*
Place an encoded version of the specified state into the buffer beginning
at next_offset, recursively adding states referenced by the transitions
from this state.  To optimize space usage in the buffer, if the state
contains only a single transition, combine the entire sequence of such
states, beginning with the current state, into a multi-character state.  As
an additional space optimization, when a transition refers to a state
containing only a single, terminal transition, replace the transition to
such a state with one giving the associated code point and do not add the
target state to the buffer.  (Note that states containing only a single,
terminal transition do not trigger creation of a multi-character state
because such terminal states are incorporated into the transition leading
to that state and thus never get here.)
*/
{
  unsigned transition_offset = next_offset;
  unsigned single_trans_states = 0;
  a_state  *last_single_trans_state = nullptr;
  a_state  *sp;

  for (sp = state; sp != NULL &&
                   sp->num_transitions == 1 &&
                   sp->transitions->name_char != '}';
       sp = sp->transitions->state) {
    ++single_trans_states;
    last_single_trans_state = sp;
  }  /* for */
  if (single_trans_states != 0) {
    /* This state is the beginning of a sequence of states in which each
       has only a single transition that will be combined into a
       multi-character state.  The space required for such a state is one
       byte giving the number of incorporated single-transition states,
       plus one byte for each of those states' transitions' characters,
       plus three bytes for the offset of the next state or, if the last
       transition is to a terminal state, the code point of the Unicode
       name that has been matched.  Update next_offset now so that data
       from a recursive invocation for the next state, if any, will follow
       the data for this state. */
    next_offset += single_trans_states + 1 + 3;
    /* The first byte of the state gives the number of characters matched
       by the state, with the high-order bit set to identify it as a
       multi-character state. */
    buffer[transition_offset++] = single_trans_states | 0x80;
    /* Walk through the single-transition states, adding the name
       characters from each to the data. */
    for (sp = state; sp != last_single_trans_state;
         sp = sp->transitions->state) {
      buffer[transition_offset++] = sp->transitions->name_char;
    }  /* for */
    buffer[transition_offset] = sp->transitions->name_char;
    if (sp->transitions->state->num_transitions == 1) {
      /* We fell out of the original loop because the next state is
         terminal.  Mark the last character of this state's sequence
         as a special transition giving the code point. */
      buffer[transition_offset] |= 0x80;
      encode_number(buffer + transition_offset + 1,
                    sp->transitions->state->transitions->code_point);
      ++a_state::terminal_states;
    } else {
      /* The transition for this state leads to a non-terminal state.  Add
         the offset for that state and recursively process it. */
      encode_number(buffer + transition_offset + 1, next_offset);
      encode_state(sp->transitions->state);
    }  /* if */
    a_state::suppressed_single_trans_states += single_trans_states - 1;
  } else if (state->num_ranges != 0) {
    /* This is a range state.  The space required is one byte giving the
       number of ranges (or'ed with 0x40 to distinguish it from normal and
       multi-character states) plus six bytes for each range (three bytes
       each for the starting and ending code points of the range). */
    next_offset += 6 * state->num_ranges + 1;
    buffer[transition_offset++] = state->num_ranges | 0x40;
    for (a_code_point_range *rp = state->code_point_ranges; rp != NULL;
         rp = rp->next) {
      encode_number(buffer + transition_offset, rp->first);
      encode_number(buffer + transition_offset + 3, rp->last);
      transition_offset += 6;
    }  /* for */
  } else {
    /* This is a normal (multi-transition) state.  The space required is
       one byte giving the number of transitions plus four bytes for each
       transition.  Update next_offset now so that data from recursive
       invocations from this state's transitions will follow the data for
       these transitions. */
    next_offset += 4 * state->num_transitions + 1;
    buffer[transition_offset++] = state->num_transitions;
    for (a_transition *transition = state->transitions; transition != nullptr;
         transition = transition->next) {
      unsigned value;
      buffer[transition_offset] = transition->name_char;
      if (transition->name_char == '}') {
        /* This is a terminal transition; add the associated code point. */
        encode_number(buffer + transition_offset + 1, transition->code_point);
        transition_offset += 4;
      } else {
        a_state *targ_state = transition->state;
        if (targ_state->num_transitions == 1 &&
            targ_state->transitions->name_char == '}') {
          /* The target state is not needed but can be encoded directly in
             this transition.  Set the high-order bit of the associated
             character to mark it as a special transition and add the
             associated code point. */
          buffer[transition_offset] |= 0x80;
          value = targ_state->transitions->code_point;
          targ_state = nullptr;
          ++a_state::terminal_states;
        } else {
          /* Add the offset at which the target state will be stored and
             recursively process the target state. */
          value = next_offset;
        }  /* if */
        encode_number(buffer + transition_offset + 1, value);
        transition_offset += 4;
        if (targ_state != nullptr) {
          encode_state(targ_state);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* encode_state */


void flatten_state_machine()
/*
Allocate sufficient storage for the flattened version of the state machine
that has been created and encode the states and transitions rooted in
initial_state into the buffer.
*/
{
  /* If not optimized, each encoded state requires one byte specifying the
     number of transitions in the state plus four bytes for each
     transition: the name character associated with the transition and the
     three bytes of the target state offset or the code point.  Each range
     occupies six bytes of storage, three each for the starting and ending
     code points.  The actual storage used will be less, due to suppression
     of states with only a single, terminal transition and sequences of
     other single-transition states. */
  buffer = new char[a_state::count + 4 * a_transition::count +
                    6 * a_code_point_range::count];
  /* Encode the initial state, which will recursively encode all states in
     the machine. */
  encode_state(initial_state);
}  /* flatten_state_machine */


/*
The invariant lines of the generated C/C++ source file.
*/
#define line_count(x) (sizeof(x) / sizeof(x[0]))
const char *header_text[] = {
"/*",
"Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM",
"Exceptions.",
"See https://edgcpp.org/LICENSE.txt for license information.",
"SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception",
"*/",
"",
"/*",
"",
"unicode_name_fsm.c -- Data for a finite state machine that recognizes",
"                      Unicode character names and supplies their associated",
"                      code points.",
"",
"*/",
"",
"#include \"basics.h\"",
"",
"/* Conditionally open the \"edg\" namespace. */",
"BEGIN_EDG_NAMESPACE",
"",
"/*",
"The following table represents a finite state machine for recognizing",
"Unicode character names and providing their associated code points.  The",
"character names are those found in UnicodeData.txt and the control,",
"correction, and alternate categories in NameAliases.txt, as well as those",
"generated algorithmically for certain character ranges in UnicodeData.txt.",
"A finite state machine implementation provides a good compromise between a",
"brute-force approach (e.g., bsearch on a table containing all the Unicode",
"character names), which would require more data, and a more complex",
"approach that would be more data efficient but would add significant",
"complexity and likely worse performance.",
"",
"The table contains states and transitions.  A transition consists of four",
"bytes.  The value of the first byte is a character that can appear in a",
"Unicode character name or '}', and the next three bytes are interpreted as",
"a number, most significant byte first.  If the character is '}' (chosen for",
"convenience because a C++23 named-universal-character is terminated by",
"'}'), the number is the Unicode code point associated with the name matched",
"by the current state.  Otherwise, the number gives the offset in the array",
"of the state to which that character transitions:",
"",
"transition:		+------------------+",
"			| name char or '}' |",
"			+------------------+",
"			|   state offset   |",
"			+--      :       --+",
"			|        or        |",
"			+--      :       --+",
"			|    code point    |",
"			+------------------+",
"",
"There are three kinds of states: normal, multi-character, and range.  A",
"normal state contains a number of transitions, one for each character that",
"can occur at that point in the name matching process, with each transition",
"designating the state resulting from matching that character.  A normal",
"state consists of a single byte containing the number of transitions in the",
"state, N, followed by N transitions:",
"",
"normal state:		+-------------------+",
"			| # transitions (N) |",
"			+-------------------+",
"			|      char #1      |",
"			+-------------------+",
"			|         :         |",
"			+--       :       --+",
"			| state/code pt #1  |",
"			+--       :       --+",
"			|         :         |",
"			+-------------------+",
"			:       . . .       :",
"			+-------------------+",
"			|      char #N      |",
"			+-------------------+",
"			|         :         |",
"			+--       :       --+",
"			| state/code pt #N  |",
"			+--       :       --+",
"			|         :         |",
"			+-------------------+",
"",
"A multi-character state is an optimization to reduce the size of the",
"machine data.  When there would be a sequence of states, each containing a",
"single transition, those states are omitted from the data and replaced by a",
"multi-character state containing the sequence of the characters for each of",
"those transitions, with the final character effectively being the head of a",
"transition to the resulting state after matching that sequence of",
"characters.  The first byte of a multi-character state gives the number of",
"characters in the sequence, or'ed with 0x80 to distinguish a",
"multi-character state from normal and range states:",
"",
"multi-char state:	+--------------------+",
"			| 0x80 + # chars (N) |",
"			+--------------------+",
"			|    name char #1    |",
"			+--------------------+",
"			:       . . .        :",
"			+--------------------+",
"			|    name char #N    |",
"			+--------------------+",
"			|         :          |",
"			+--       :        --+",
"			|    state offset    |",
"			+--       :        --+",
"			|         :          |",
"			+--------------------+",
"",
"As another optimization, when a state would contain only a terminal",
"transition (i.e., whose character is '}', ending a name and giving its code",
"point), that state is omitted from the table; instead, the character in",
"the transition that would have designated that state is or'ed with 0x80",
"(which is not a conflict, since the characters in Unicode names are all",
"ASCII) and the number in the transition is the code point associated with",
"the name instead of the offset of a state.  This optimization applies to",
"both transitions from normal states and to the final character of a",
"multi-character state:",
"",
"optimized transition:	+-------------------+",
"			| 0x80 +  name char |",
"			+-------------------+",
"			|         :         |",
"			+--       :       --+",
"			|    code point     |",
"			+--       :       --+",
"			|         :         |",
"			+-------------------+",
"",
"A range state represents one or more ranges of characters in which the names",
"all share a common prefix and the last 4 or 5 bytes of the name are an",
"ASCII representation of the character's hexadecimal code point.  E.g., the",
"three characters FOO-ABC1, FOO-ABC2, and FOO-ABC3 with values in the range",
"from 0xABC1 to 0xABC3 could be represented by a range state, which consists",
"of one byte giving the number of ranges represented (or'ed with 0x40, i.e.,",
"yielding 0x41 in this case with one range), followed by a single range",
"consisting of the three bytes for 0xABC1 and the three bytes for 0xABC3.",
"(Such a state would be reached by the transition for '-' after having",
"matched \"FOO\".):",
"",
"range state:		+---------------------+",
"			| 0x40 + # ranges (N) |",
"			+---------------------+",
"			|          :          |",
"			+--  first code pt  --+",
"			|         for         |",
"			+--    range #1     --+",
"			|          :          |",
"			+---------------------+",
"			|          :          |",
"			+--  last code pt   --+",
"			|         for         |",
"			+--     range #1    --+",
"			|          :          |",
"			+---------------------+",
"			:        . . .        :",
"			+---------------------+",
"			|          :          |",
"			+--  first code pt  --+",
"			|         for         |",
"			+--    range #N     --+",
"			|          :          |",
"			+---------------------+",
"			|          :          |",
"			+--  last code pt   --+",
"			|         for         |",
"			+--     range #N    --+",
"			|          :          |",
"			+---------------------+",
"",
"For example, if the Unicode character set consisted of four characters with",
"names \"A\", \"AB\", \"CDEF\", and \"CDEG\" corresponding to code points 0x20,",
"0x21, 0x22, and 0x23, respectively, the resulting table would be:",
"",
"offset  value       meaning",
"------  -----       -------",
"   0:      2        [initial state; 2 transitions]",
"          'A'       [transition on 'A']",
"           0 \\",
"           0  >--   [designates state at offset 9]",
"           9 /",
"          'C'       [transition on 'C']",
"           0 \\",
"           0  >--   [designates state at offset 18]",
"          18 /",
"   9:      2        [state after \"A\"; 2 transitions]",
"          '}'       [terminal transition for name \"A\"]",
"           0 \\",
"           0  >--   [code point 0x20]",
"        0x20 /",
"        0xC2        [transition on 'B' (ASCII 0x42 + end of name flag)]",
"           0 \\",
"           0  >--   [code point 0x21]",
"        0x21 /",
"  18:   0x82        [multi-character state after \"C\" with 2 characters]",
"          'D'",
"          'E'",
"           0 \\",
"           0  >--   [designates state at offset 24]",
"          24 /",
"  24:      2        [state after \"CDE\"; 2 transitions]",
"        0xC6        [transition on 'F' (ASCII 0x46 + end of name flag)]",
"           0 \\",
"           0  >--   [code point 0x22]",
"        0x22 /",
"        0xC7        [transition on 'G' (ASCII 0x47 + end of name flag)]",
"           0 \\",
"           0  >--   [code point 0x23]",
"        0x23 /",
"*/",
"",
"extern const unsigned char unicode_name_fsm[] = {",
"",
"/* The following table is created by a tool that processes the Unicode data",
"   files and is not intended to be edited manually.  This table reflects"
};  /* header_text */


const char *footer_text[] = {
"}; /* unicode_name_fsm */",
"",
"extern const sizeof_t size_of_unicode_name_fsm = sizeof(unicode_name_fsm);",
"",
"/* Conditionally close the \"edg\" namespace. */",
"END_EDG_NAMESPACE",
""
};  /* footer_text */

void dump_state_machine(FILE       *output_file,
                        const char *unicode_version,
                        const char *unicode_date)
/*
Write the flattened form of the state machine as a C/C++ source file
containing a variable that is an array of unsigned char initialized with a
list of one-byte hexadecimal integers, suitable for initializing a C/C++
character array, to output_file, which is open for writing.
*/
{
  int i;

  /* Put out the invariant text for the top of the file. */
  for (i = 0; i < line_count(header_text); ++i) {
    fprintf(output_file, "%s\n", header_text[i]);
  }  /* for */
  /* Add the Unicode version and date. */
  fprintf(output_file, "   Unicode version %s, dated %s. */\n\n",
          unicode_version, unicode_date);
  /* Put out the table, eight bytes per line, giving offsets every 128
     bytes. */
  for (i = 0; i < next_offset; ++i) {
    if (i % 8 == 0) {
      if (i % 128 == 0) {
        fprintf(output_file, "  /* %06x: */\t", i);
      } else {
        fprintf(output_file, "\t\t");
      }  /* if */
    } else {
      fprintf(output_file, " ");
    }  /* if */
    fprintf(output_file, "0x%02x", ((unsigned)buffer[i]) & 0xff);
    if (i != next_offset - 1) {
      fprintf(output_file, ",");
      if (i % 8 == 7) {
        fprintf(output_file, "\n");
      }  /* if */
    } else {
      fprintf(output_file, "\n");
    }  /* if */
  }  /* for */
  /* Put out the invariant text for the bottom of the file. */
  for (i = 0; i < line_count(footer_text); ++i) {
    fprintf(output_file, "%s\n", footer_text[i]);
  }  /* for */
}  /* dump_state_machine */


int main(int argc, char *argv[]) {
FILE	*data_file = nullptr;
FILE	*alias_file = nullptr;
FILE	*output_file;

  if (argc != 6) {
    report_error_and_exit(
                 "Usage: %s data-file aliases-file output-file version date\n",
                 argv[0]);
  }  /* if */
  /* Open input and output files. */
  if ((data_file = fopen(argv[1], "r")) == NULL) {
    report_error_and_exit("Cannot open %s for input\n", argv[1]);
  }  /* if */
  if ((alias_file = fopen(argv[2], "r")) == NULL) {
    report_error_and_exit("Cannot open %s for input\n", argv[2]);
  }  /* if */
  if ((output_file = fopen(argv[3], "w")) == NULL) {
    report_error_and_exit("Cannot open %s for output\n", argv[3]);
  }  /* if */
  /* Create the state machine. */
  process_data_file(data_file);
  process_alias_file(alias_file);
  /* Create the output data file. */
  flatten_state_machine();
  dump_state_machine(output_file, argv[4], argv[5]);
  fclose(output_file);
  /* Display some interesting statistics. */
  fprintf(stderr,
          "STATISTICS: %u states (%u terminal + %u single-trans suppressed), "
          "%u transitions;\n",
          a_state::count, a_state::terminal_states,
          a_state::suppressed_single_trans_states, a_transition::count);
  fprintf(stderr, "size of buffer = %u bytes.\n", next_offset);
}  /* main */
