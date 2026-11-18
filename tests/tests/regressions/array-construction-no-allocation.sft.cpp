//type:fp
//options_all:--c++11
//options:--exceptions:--no_exceptions
//require:BACK_END_IS_C_GEN_BE 1
//filter:cat; grep -Eq '__array_new|__vec_new|__cxa_vec_(new|ctor)' Test_name.int.c; [ $? = 1 ] && echo 'array construction needs no allocation helper'
//match_regex:array construction needs no allocation helper

struct Element {
  int value;
  Element(int n = 7) : value(n) {}
#if TEST_NUMBER == 2
  ~Element();
#endif
};

struct Container {
  Element elements[2][3];
  Container();
};

Container::Container() {}

void construct() {
  Container object;
}
