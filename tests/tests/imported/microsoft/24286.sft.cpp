//options_all:--xref x --c++11
//filter:cat x | sed 's/^0x[0-9a-fA-F]*/<address>/' | edg-normalize-test-output
template <int T>
struct Foo : Foo<T-1> {};
