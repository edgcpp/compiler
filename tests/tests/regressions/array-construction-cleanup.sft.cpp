//type:fp
//options_all:--c++11 --exceptions
//require:BACK_END_IS_C_GEN_BE 1
//filter:cat; grep -Eq '__edg_vec_ctor|__cxa_vec_ctor' Test_name.int.c && ! grep -Eq '__vec_new_eh|__cxa_vec_new' Test_name.int.c && echo 'exception cleanup helper retained'
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
