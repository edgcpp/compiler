//type:fp
//options_all:--c++11 -A -tused
//remark:[GH #592] Range-based for uses ADL without ordinary lookup.

namespace associated_lookup {
  struct Range {
    int values[1];
    friend int *begin(Range& r) { return r.values; }
    friend int *end(Range& r) { return r.values + 1; }
  };
}

/* ADL finds the hidden friends; ordinary lookup finds deleted functions. */
namespace ordinary_lookup_ignored {
  int *begin(associated_lookup::Range&) = delete;
  int *end(associated_lookup::Range&) = delete;

  template<typename T> void iterate(T& range) {
    for (int value : range) (void)value;
  }

  void test(associated_lookup::Range& range) { iterate(range); }
}

namespace std {
  struct Range {};
  int *begin(Range&);
  int *end(Range&);
}

/* std participates in ADL when it is associated with the range type. */
namespace associated_std {
  void test(std::Range& range) {
    for (int value : range) (void)value;
  }
}

//cwg: 1442
//title: Argument-dependent lookup in the range-based for
//meeting: Bristol 4/13
//edg_status: GH #592
//fixed_in: 7.1
