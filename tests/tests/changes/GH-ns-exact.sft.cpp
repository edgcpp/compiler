//type:fp
//options:--c++17
//remark:[GH #305] Attribute namespaces that only start like a known one
//match_regex:attribute namespace "cl" is unrecognized
//match_regex:attribute namespace "gn" is unrecognized
//match_regex:attribute namespace "g" is unrecognized
//match_regex:attribute namespace "ms" is unrecognized
//match_regex:attribute namespace "_Cl" is unrecognized
//match_regex:\A(?![\s\S]*attribute namespace "(gnu|__gnu__|clang|_Clang|msvc)" is unrecognized)
// 10/14/26  [GH #305]
//
// A namespace that is only a prefix of a namespace known to the front end
// ("clang", "gnu", "msvc", "_Clang") is not recognized: it gets the
// "attribute namespace is unrecognized" warning like any other unknown one.
// The known namespaces themselves are still recognized: they get the
// "unknown attribute" warning for the unknown attribute name only (the last
// regular expression above fails if one of them is reported as unrecognized).

[[cl::x]] int a;
[[gn::y]] int b;
[[g::z]] int c;
[[ms::w]] int d;
[[_Cl::v]] int e;

[[gnu::x]] int f;
[[__gnu__::x]] int g;
[[clang::x]] int h;
[[msvc::x]] int i;
[[_Clang::x]] int j;
