//type:fp
//options_all:--c++11 --exceptions
//require:BACK_END_IS_C_GEN_BE 1
//filter:cat; awk 'BEGIN { found = 0 } /__vec_new_eh|__cxa_vec_ctor/ { found = 1 } END { if (found) print "exception cleanup helper retained"; else print "missing exception cleanup helper" }' Test_name.int.c
//match_regex:exception cleanup helper retained

struct Element {
  Element();
  ~Element();
};

struct Container {
  Element elements[2];
  Container();
};

Container::Container() {}
