//type:cp
//options_all:--ms_c++20 --microsoft_v 1930
namespace std {
    template <class, class>
    inline constexpr bool is_same_v = false;
    template <class T>
    inline constexpr bool is_same_v<T, T> = true;

    template <class B, class D>
    inline constexpr bool is_base_of_v = __is_base_of(B, D);

    template <class T>
    inline constexpr bool is_abstract_v = __is_abstract(T);

    struct source_location {
        static consteval source_location current(
            unsigned _Line_ = __builtin_LINE(),
            unsigned _Column_ = __builtin_COLUMN(),
            const char *_File_ = __builtin_FILE(),
            const char *_Function_ = __builtin_FUNCTION()
        ) noexcept {
            source_location _Result{};
            _Result._Line     = _Line_;
            _Result._Column   = _Column_;
            _Result._File     = _File_;
            _Result._Function = _Function_;
            return _Result;
        }

    private:
        unsigned _Line{};
        unsigned _Column{};
        const char *_File = "";
        const char *_Function = "";
    };
}

struct IUnknown { virtual void QueryInterface() = 0; };
void *_ReturnAddress();

template <class>
class com_ptr {};

template <class T, class I>
inline constexpr bool is_or_implements_v = std::is_same_v<T, I> || std::is_base_of_v<I, T>;

template <class T>
concept com_interface = std::is_abstract_v<T> && is_or_implements_v<T, IUnknown>;

template <class>
struct ptr_traits;

template <com_interface T>
struct ptr_traits<com_ptr<T>> { static void Throw(std::source_location, void *); };

template <class T>
concept smart_ptr = requires {
    ptr_traits<T>::Throw(std::source_location::current(), _ReturnAddress());
};

template <smart_ptr T, smart_ptr U>
void qi_cast(U &, std::source_location = std::source_location::current());

struct IFoo : IUnknown {};
struct IBaz : IUnknown {};

void f(com_ptr<IFoo> foo) {
    qi_cast<com_ptr<IBaz>>(foo);
}
