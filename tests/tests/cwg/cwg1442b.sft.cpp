//type:fn
//options_all:--c++11 -A
//remark:[GH #592] Range-based for rejects bounds found outside ADL.

namespace unassociated_std {
  struct Range {};
}

/* std is not associated with unassociated_std::Range. */
namespace std {
  int *begin(unassociated_std::Range&);
  int *end(unassociated_std::Range&);
}

namespace unassociated_std {
  void test(Range& range) {
    for (int value : range) (void)value;
  }
}

namespace no_adl_bounds {
  struct Range {};
}

/* Ordinary unqualified lookup finds these functions, but ADL does not. */
namespace ordinary_lookup_only {
  int *begin(no_adl_bounds::Range&);
  int *end(no_adl_bounds::Range&);

  void test(no_adl_bounds::Range& range) {
    for (int value : range) (void)value;
  }
}
