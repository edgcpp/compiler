//options_all:--c++20
//type:fp
struct Thing { int a; };

template <Thing A>
struct B {};

B<Thing{ 1 }> b1;
B<{ 1 }> b2;
