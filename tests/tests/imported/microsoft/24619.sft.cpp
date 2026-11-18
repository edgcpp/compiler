//options_all:--c++20
template <auto, auto> class T3;
template <class U, class V, U U::* x, V V::* y> class T3<x, y> {};

template <auto, auto> class T4;
template <class U, class V, U V::* x, U U::* y> class T4<x, y> {};

template <auto, auto> class T7;
template <class U, class V, U V::* x, V V::* y> class T7<x, y> {};

template <auto, auto> class T8;
template <class U, class V, V U::* x, U U::* y> class T8<x, y> {};

template <auto, auto> class T11;
template <class U, class V, V U::* x, V V::* y> class T11<x, y> {};

template <auto, auto> class T12;
template <class U, class V, V V::* x, U U::* y> class T12<x, y> {};
