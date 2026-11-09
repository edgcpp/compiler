//options:--microsoft --c++17

// A discarded unbraced "__try ... __finally" substatement of an
// "if constexpr" must be flushed as exactly one statement.  The lambda is
// needed so that the instantiation flushes the discarded tokens rather than
// skipping to positions recorded for the enclosing function template.  If
// a "return" were discarded, the lambda's return type would be deduced as
// void and the "return" in the enclosing function would be an error.

template <bool B> int then_discarded() {
  return [] {
    int n = 0;
    if constexpr (B) __try { n = 100; } __finally { n = 200; }
    return n;
  }();
}
template int then_discarded<false>();

template <bool B> int else_discarded() {
  return [] {
    int n = 0;
    if constexpr (!B) n = 1; else __try { n = 100; } __finally { n = 200; }
    return n;
  }();
}
template int else_discarded<false>();

template <bool B> int nested() {
  return [] {
    int n = 0;
    if constexpr (B) while (B) __try { n = 100; } __finally { n = 200; }
    return n;
  }();
}
template int nested<false>();
