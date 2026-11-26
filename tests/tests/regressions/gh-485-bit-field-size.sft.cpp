//type:fp
//options_all:--c++20
//options:--g++ --gnu_version=130300:--strict
// An enum with a fixed underlying type gives its bit fields the signedness of
// that type, whether the enum is scoped or has an explicit enum base, so both
// of these bit fields are too small to hold the value 3.
enum class F { f3 = 3 };
struct S { F f : 2; };
enum E : int { e3 = 3 };
struct T { E e : 2; };
