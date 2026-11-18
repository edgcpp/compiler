//type:fp
//options_all:--c++11
//options:--exceptions:--no_exceptions
//require:BACK_END_IS_C_GEN_BE 1
//filter:cat; awk 'BEGIN { found = 0 } /__array_new|__vec_new|__cxa_vec_new/ { found = 1 } END { if (found) print "dynamic allocation helper retained"; else print "missing dynamic allocation helper" }' Test_name.int.c
//match_regex:dynamic allocation helper retained

struct Element {
  Element();
  ~Element();
};

Element *allocate() {
  return new Element[2];
}
