//options_all:--c++20
struct string_view {
    string_view(const string_view &) = default;

    template <class R> requires (!requires(R rng) { rng.operator string_view(); })
    string_view(R &&) {}
};

string_view _Get_integral_prefix(string_view sv) {
    return sv;
}
