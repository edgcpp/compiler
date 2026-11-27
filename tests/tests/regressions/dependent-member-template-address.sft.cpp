//type:fp
//options:--c++20:--c++20 --gnu_version 150300:--c++20 --clang_version 190100

template<class...> using void_t = void;
template<class T, class = void> constexpr bool has_f = false;
template<class T> constexpr bool has_f<T, void_t<decltype(&T::template f<>)>> =
  true;

struct Plain { int f() const; };
struct Generic { template<class = void> int f() const; };
struct Data { int f; };
struct StaticData { static int f; };
struct StaticGeneric { template<class = void> static int f; };
static_assert(!has_f<Plain>);
static_assert(!has_f<Data>);
static_assert(!has_f<StaticData>);
static_assert(has_f<Generic>);
static_assert(has_f<StaticGeneric>);
