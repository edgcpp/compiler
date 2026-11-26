//type:cp
//options_all:--microsoft_v 1950 --ms_c++20
template<class T>
struct S {};

template <class T>
concept DerivedFromS = requires(const T & t) { [] <class U>(const S<U>&) {}(t); };

inline constexpr auto DetectionLambda = []<class U>(const S<U>&) {};

template <class T>
concept DerivedFromSWorkaround = requires(const T & t) { DetectionLambda(t); };

template <class T>
struct SD : S<T> {};

static_assert(DerivedFromS<SD<int>>);           // rejected by EDG
static_assert(DerivedFromSWorkaround<SD<int>>); // accepted by all implementations
