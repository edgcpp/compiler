//type:fn
//options_all:--c++17
template <class T> struct _A {};
void f() { throw _A ( _A <int> ); }
