//type:cp
//options_all:--microsoft_v 1940 --ms_c++20
#if __has_cpp_attribute(no_unique_address)
#define NO_UNIQUE_ADDRESS [[no_unique_address]] // EDG selects this.
#elif __has_cpp_attribute(msvc::no_unique_address)
#define NO_UNIQUE_ADDRESS [[msvc::no_unique_address]] // MSVC and Clang-cl select this.
#elif defined(__INTEL_COMPILER) // ICC is not properly supported.
#define NO_UNIQUE_ADDRESS
#else
#error Either [[msvc::no_unique_address]] or [[no_unique_address]] must be supported because this is ABI-critical.
#endif

struct Empty {};
struct EmptyHolder {
    NO_UNIQUE_ADDRESS Empty e;
};

template <class _Ty>
constexpr bool is_empty_v = __is_empty(_Ty);

static_assert(is_empty_v<Empty>); // OK with MSVC, Clang, and EDG
static_assert(is_empty_v<EmptyHolder>);

int main()
{
    return 0;
}
