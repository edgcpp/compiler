//type:fp
//options_all:--c++11 --exceptions
//require:BACK_END_IS_C_GEN_BE 1
//filter:cat; grep -Eq '__edg_vec_cctor|__cxa_vec_cctor' Test_name.int.c && ! grep -Eq '__vec_cctor_eh|__cxa_vec_new' Test_name.int.c && echo 'nonallocating array copy'
//match_regex:nonallocating array copy

struct Element {
  Element();
  Element(const Element &);
  ~Element();
};

struct Holder {
  Element elements[3];
};

Holder copy(const Holder &source) { return source; }
