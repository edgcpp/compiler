//options_all:--microsoft -E
#define PP_CAT(A, B) A ## B
#define PP_EXPAND(...) __VA_ARGS__

#define PP_VA_ARG_SIZE(...) PP_EXPAND(PP_APPLY_ARG_N((PP_ZERO_ARGS_DETECT(__VA_ARGS__), PP_RSEQ_N)))

#define PP_ZERO_ARGS_DETECT(...) PP_EXPAND(PP_ZERO_ARGS_DETECT_PREFIX_ ## __VA_ARGS__ ## _ZERO_ARGS_DETECT_SUFFIX)
#define PP_ZERO_ARGS_DETECT_PREFIX__ZERO_ARGS_DETECT_SUFFIX ,,,,,,,,,,,0

#define PP_APPLY_ARG_N(ARGS) PP_EXPAND(PP_ARG_N ARGS)
#define PP_ARG_N(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N,...) N
#define PP_RSEQ_N 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

#define PP_OVERLOAD_SELECT(NAME, NUM) PP_CAT(NAME ## _, NUM)
#define PP_MACRO_OVERLOAD(NAME, ...) PP_OVERLOAD_SELECT(NAME, PP_VA_ARG_SIZE(__VA_ARGS__))(__VA_ARGS__)

#define ASSERT_2(condition, message) \
              do { \
                             if (!(condition)) { \
                                           std::cout << "Assertion on condition " << #condition << " with message " << message << "\n"; \
                                           abort(); \
                             } \
              } while(false)

#define ASSERT_1(condition) \
              do { \
                             if (!(condition)) { \
                                           std::cout << "Assertion on condition " << #condition << "\n"; \
                                           abort(); \
                             } \
              } while(false)

#define ASSERT(...) PP_MACRO_OVERLOAD(ASSERT, __VA_ARGS__)

int main() {
              ASSERT(true);
              ASSERT(false, "Condition cannot be false");
}
