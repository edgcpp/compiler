//type:fp
//options_all:--c++11
//options:--exceptions:--no_exceptions
//require:BACK_END_IS_C_GEN_BE 1
//filter:cat; grep -Eq '__array_new|__vec_new|__cxa_vec_(new|ctor)' Test_name.int.c; [ $? = 1 ] && echo 'no allocation helper'
//match_regex:no allocation helper

struct Member {
  int value;
  Member() : value(42) {}
};

struct Element {
  int value;
  Member member;
  Element() = default;
};

struct Container {
  Element elements[2];
  Container() : elements{} {}
};

Container make() { return Container(); }
