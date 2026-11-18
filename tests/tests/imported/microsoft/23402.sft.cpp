//type:fn
//options_all:--c++20 -A
//options::-DFOO
   struct D { D(); };
   struct C { explicit operator D(); } c;

#ifdef FOO
   const D &d2{c};     // should be an error
#else
   const D &d3(c);     // should be an error
#endif
