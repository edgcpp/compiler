//options_all:--microsoft_version 1929 --ms_c++latest
struct source_location {
    static consteval source_location current(
        unsigned int in_line = __builtin_LINE(),
        unsigned int in_column = __builtin_COLUMN(),
        const char* in_file = __builtin_FILE(),
        const char* in_function = __builtin_FUNCTION()
    ) noexcept {
        source_location result;
        result.line = in_line;
        result.column = in_column;
        result.file = in_file;
        result.function = in_function;
        return result;
    }

    constexpr source_location() noexcept = default;

    unsigned int line = 0;
    unsigned int column = 0;
    const char* file = "";
    const char* function = "";
};

struct S {
    source_location loc = source_location::current();
};

constexpr unsigned int argument_test(source_location arg = source_location::current()) {
    return arg.column;
}
