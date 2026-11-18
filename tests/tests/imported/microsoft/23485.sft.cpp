//type:fn
//options:--c++14:--c++17
struct S0 { ~S0() noexcept(false); };
struct S1 { virtual ~S1() noexcept(true); };
struct S : S1 {
union { S0 s; };
};
