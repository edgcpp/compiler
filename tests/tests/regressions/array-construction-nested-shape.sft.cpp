//type:fp
//options_all:--c++11
//options:--exceptions:--no_exceptions
//require:BACK_END_IS_C_GEN_BE 1
//filter:cat; grep -Eq 'struct Element \(\*\)\[3\]\[4\]' Test_name.int.c && grep -Eq 'struct Element \(\*\)\[4\]' Test_name.int.c && echo 'nested array traversal'
//match_regex:nested array traversal

struct Element {
  int value;
  Element() : value(1) {}
};

void construct() {
  Element elements[2][3][4];
}
