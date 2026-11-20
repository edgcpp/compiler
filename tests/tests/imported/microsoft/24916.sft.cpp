//options_all:--c++20
namespace std {
    using _Literal_zero = decltype(nullptr);
    using _Compare_t    = signed char;
    enum class _Compare_eq : _Compare_t { equal = 0, equivalent = equal };
    enum class _Compare_ord : _Compare_t { less = -1, greater = 1 };
    enum class _Compare_ncmp : _Compare_t { unordered = -128 };

    struct partial_ordering {
        static const partial_ordering less;
        static const partial_ordering equivalent;
        static const partial_ordering greater;
        static const partial_ordering unordered;

        friend constexpr bool operator==(const partial_ordering _Val, _Literal_zero) noexcept { return _Val._Value == 0; }
        friend constexpr bool operator==(partial_ordering, partial_ordering) noexcept = default;
        friend constexpr bool operator<(const partial_ordering _Val, _Literal_zero) noexcept { return _Val._Value == static_cast<_Compare_t>(_Compare_ord::less); }
        friend constexpr bool operator>(const partial_ordering _Val, _Literal_zero) noexcept { return _Val._Value > 0; }
        friend constexpr bool operator<=(const partial_ordering _Val, _Literal_zero) noexcept { return static_cast<signed char>(0 - static_cast<unsigned int>(_Val._Value)) >= 0; }
        friend constexpr bool operator>=(const partial_ordering _Val, _Literal_zero) noexcept { return _Val._Value >= 0; }
        friend constexpr bool operator<(_Literal_zero, const partial_ordering _Val) noexcept { return _Val > 0; }
        friend constexpr bool operator>(_Literal_zero, const partial_ordering _Val) noexcept { return _Val < 0; }
        friend constexpr bool operator<=(_Literal_zero, const partial_ordering _Val) noexcept { return _Val >= 0; }
        friend constexpr bool operator>=(_Literal_zero, const partial_ordering _Val) noexcept { return _Val <= 0; }
        friend constexpr partial_ordering operator<=>(const partial_ordering _Val, _Literal_zero) noexcept { return _Val; }
        friend constexpr partial_ordering operator<=>(_Literal_zero, const partial_ordering _Val) noexcept { return {static_cast<_Compare_t>(0 - static_cast<unsigned int>(_Val._Value))}; }

        _Compare_t _Value;
    };
    inline constexpr partial_ordering partial_ordering::less{static_cast<_Compare_t>(_Compare_ord::less)};
    inline constexpr partial_ordering partial_ordering::equivalent{static_cast<_Compare_t>(_Compare_eq::equivalent)};
    inline constexpr partial_ordering partial_ordering::greater{static_cast<_Compare_t>(_Compare_ord::greater)};
    inline constexpr partial_ordering partial_ordering::unordered{static_cast<_Compare_t>(_Compare_ncmp::unordered)};

    struct weak_ordering {
        static const weak_ordering less;
        static const weak_ordering equivalent;
        static const weak_ordering greater;

        constexpr operator partial_ordering() const noexcept { return {static_cast<_Compare_t>(_Value)}; }
        friend constexpr bool operator==(const weak_ordering _Val, _Literal_zero) noexcept { return _Val._Value == 0; }
        friend constexpr bool operator==(weak_ordering, weak_ordering) noexcept = default;
        friend constexpr bool operator<(const weak_ordering _Val, _Literal_zero) noexcept { return _Val._Value < 0; }
        friend constexpr bool operator>(const weak_ordering _Val, _Literal_zero) noexcept { return _Val._Value > 0; }
        friend constexpr bool operator<=(const weak_ordering _Val, _Literal_zero) noexcept { return _Val._Value <= 0; }
        friend constexpr bool operator>=(const weak_ordering _Val, _Literal_zero) noexcept { return _Val._Value >= 0; }
        friend constexpr bool operator<(_Literal_zero, const weak_ordering _Val) noexcept { return _Val > 0; }
        friend constexpr bool operator>(_Literal_zero, const weak_ordering _Val) noexcept { return _Val < 0; }
        friend constexpr bool operator<=(_Literal_zero, const weak_ordering _Val) noexcept { return _Val >= 0; }
        friend constexpr bool operator>=(_Literal_zero, const weak_ordering _Val) noexcept { return _Val <= 0; }
        friend constexpr weak_ordering operator<=>(const weak_ordering _Val, _Literal_zero) noexcept { return _Val; }
        friend constexpr weak_ordering operator<=>(_Literal_zero, const weak_ordering _Val) noexcept { return {static_cast<_Compare_t>(-_Val._Value)}; }

        _Compare_t _Value;
    };

    inline constexpr weak_ordering weak_ordering::less{static_cast<_Compare_t>(_Compare_ord::less)};
    inline constexpr weak_ordering weak_ordering::equivalent{static_cast<_Compare_t>(_Compare_eq::equivalent)};
    inline constexpr weak_ordering weak_ordering::greater{static_cast<_Compare_t>(_Compare_ord::greater)};

    struct strong_ordering {
        static const strong_ordering less;
        static const strong_ordering equal;
        static const strong_ordering equivalent;
        static const strong_ordering greater;

        constexpr operator partial_ordering() const noexcept { return {static_cast<_Compare_t>(_Value)}; }
        constexpr operator weak_ordering() const noexcept { return {static_cast<_Compare_t>(_Value)}; }
        friend constexpr bool operator==(const strong_ordering _Val, _Literal_zero) noexcept { return _Val._Value == 0; }
        friend constexpr bool operator==(strong_ordering, strong_ordering) noexcept = default;
        friend constexpr bool operator<(const strong_ordering _Val, _Literal_zero) noexcept { return _Val._Value < 0; }
        friend constexpr bool operator>(const strong_ordering _Val, _Literal_zero) noexcept { return _Val._Value > 0; }
        friend constexpr bool operator<=(const strong_ordering _Val, _Literal_zero) noexcept { return _Val._Value <= 0; }
        friend constexpr bool operator>=(const strong_ordering _Val, _Literal_zero) noexcept { return _Val._Value >= 0; }
        friend constexpr bool operator<(_Literal_zero, const strong_ordering _Val) noexcept { return _Val > 0; }
        friend constexpr bool operator>(_Literal_zero, const strong_ordering _Val) noexcept { return _Val < 0; }
        friend constexpr bool operator<=(_Literal_zero, const strong_ordering _Val) noexcept { return _Val >= 0; }
        friend constexpr bool operator>=(_Literal_zero, const strong_ordering _Val) noexcept { return _Val <= 0; }
        friend constexpr strong_ordering operator<=>(const strong_ordering _Val, _Literal_zero) noexcept { return _Val; }
        friend constexpr strong_ordering operator<=>(_Literal_zero, const strong_ordering _Val) noexcept { return {static_cast<_Compare_t>(-_Val._Value)}; }

        _Compare_t _Value;
    };

    inline constexpr strong_ordering strong_ordering::less{static_cast<_Compare_t>(_Compare_ord::less)};
    inline constexpr strong_ordering strong_ordering::equal{static_cast<_Compare_t>(_Compare_eq::equal)};
    inline constexpr strong_ordering strong_ordering::equivalent{static_cast<_Compare_t>(_Compare_eq::equivalent)};
    inline constexpr strong_ordering strong_ordering::greater{static_cast<_Compare_t>(_Compare_ord::greater)};
}

using Byte = unsigned char;
using UInt32 = unsigned int;
using UInt64 = unsigned long long;

struct CCount {
    UInt64 Value;

    constexpr CCount(UInt64 value) : Value{ value } {}
    constexpr auto operator<=>(const CCount &) const = default;
};

inline constexpr CCount operator""_count(UInt64 value) {
    return value;
}

template <class Type, CCount CountOfElements> requires (CountOfElements > 0_count)
class CArray {};

enum class EInstruction { JumpAbsoluteIndirect };

template <EInstruction Instruction>
union UInstruction;

template<>
union UInstruction<EInstruction::JumpAbsoluteIndirect> {
    CArray<Byte, CCount{ sizeof(Byte) + sizeof(Byte) + sizeof(UInt32) }> Bytes{ };
};
