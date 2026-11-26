//type:fp
//options_all:--c++11
//options:--exceptions:--no_exceptions
//require:BACK_END_IS_C_GEN_BE 1
//filter:cat; grep -Eq '__array_new|__vec_new|__cxa_vec_new' Test_name.int.c && echo 'dynamic allocation helper retained'
//match_regex:dynamic allocation helper retained

struct Element {
  Element();
  ~Element();
};

Element *allocate() {
  return new Element[2];
}
