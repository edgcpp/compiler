//type:fp
//options:--c++11
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

template<int N> struct C : C<N-1> {};
template<> struct C<0> { };
C<3> c;

/* @filter
file-scope base-class-derivation
  path
 */
