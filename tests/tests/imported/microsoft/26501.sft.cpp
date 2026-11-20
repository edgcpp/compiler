//type:cp
//options_all:--ms_c++20 --microsoft_v 1940
namespace std {
struct source_location {
    static consteval source_location current(
        const unsigned _Line_ = __builtin_LINE(),
        const unsigned _Column_ = __builtin_COLUMN(),
        const char* const _File_ = __builtin_FILE(),
        const char* const _Function_ = __builtin_FUNCTION()
    ) noexcept {
        source_location _Result{};
        _Result._Line     = _Line_;
        _Result._Column   = _Column_;
        _Result._File     = _File_;
        _Result._Function = _Function_;
        return _Result;
    }

    constexpr source_location() noexcept = default;

private:
    unsigned _Line{};
    unsigned _Column{};
    const char* _File     = "";
    const char* _Function = "";
};
}

template <class F>
constexpr auto invoke(F &&f) noexcept(noexcept(static_cast<F &&>(f)())) -> decltype(static_cast<F &&>(f)()) {
    return static_cast<F &&>(f)();
}

void test() {
    invoke([](std::source_location = std::source_location::current()) {});
}
