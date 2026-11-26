//options_all:--c++20 --microsoft
//type:fp
namespace std {
    using _Literal_zero = decltype(nullptr);
    using _Compare_t = signed char;

    enum class _Compare_ord : _Compare_t { less = -1, greater = 1 };

    class strong_ordering {
    public:
        constexpr explicit strong_ordering(const _Compare_ord _Value_) noexcept
            : _Value(static_cast<_Compare_t>(_Value_)) {}

        static const strong_ordering less;
        static const strong_ordering greater;

        friend constexpr bool operator<(const strong_ordering _Val, _Literal_zero) noexcept {
            return _Val._Value < 0;
        }

        friend constexpr bool operator>(const strong_ordering _Val, _Literal_zero) noexcept {
            return _Val._Value > 0;
        }

        friend constexpr strong_ordering operator<=>(const strong_ordering _Val, _Literal_zero) noexcept {
            return _Val;
        }

    private:
        _Compare_t _Value;
    };

    inline constexpr strong_ordering strong_ordering::less(_Compare_ord::less);
    inline constexpr strong_ordering strong_ordering::greater(_Compare_ord::greater);
}

constexpr auto sol = std::strong_ordering::less;
constexpr auto sog = std::strong_ordering::greater;

static_assert((sol <=> 0) < 0);
static_assert((sog <=> 0) > 0);
static_assert(sol <=> 0 < 0);
static_assert(sog <=> 0 > 0);
