/**
 * @file test_types_numerics_exec.cpp
 * @brief Native C++20 execution test for `std::nullptr_t`, null PMD (`-1` vs offset `0`), PMF, and `source_location`.
 * @details Verifies `std::nullptr_t` passing/returning, null pointer-to-data-member (`-1` distinct from
 * the first field at byte offset `0`), virtual pointer-to-member-function dispatch, and `conv_seq_to_file_and_line`
 * resolution of `__builtin_FILE`, `__builtin_LINE`, `__builtin_FUNCTION`, and `std::source_location::current()`.
 * @ingroup gcc_gen_be_tests
 */

#include <source_location>
#include "test_common.h"

namespace {

/**
 * @brief Test struct whose first field `first_at_zero` is at byte offset `0`.
 */
struct PmdHost {
    /** @brief First data member at byte offset `0` (must NOT compare equal to a null PMD!). */
    int first_at_zero;
    /** @brief Second data member at byte offset `4`. */
    int second_at_four;
};

/**
 * @brief Polymorphic class testing virtual and non-virtual pointers-to-member-functions.
 */
struct PmfBase {
    /** @brief Stored base value. */
    int val;
    /**
     * @brief Constructs `PmfBase`.
     * @param[in] v Initial value.
     */
    explicit PmfBase(int v) noexcept : val(v) {}
    /** @brief Virtual destructor. */
    virtual ~PmfBase() noexcept {}
    /**
     * @brief Virtual method invoked through PMF.
     * @param[in] delta Increment.
     * @return `val + delta`.
     */
    virtual int compute(int delta) const noexcept { return val + delta; }
};

/**
 * @brief Derived class overriding `PmfBase::compute`.
 */
struct PmfDerived : public PmfBase {
    /**
     * @brief Constructs `PmfDerived`.
     * @param[in] v Initial value.
     */
    explicit PmfDerived(int v) noexcept : PmfBase(v) {}
    /**
     * @brief Overrides `compute` to return `val + delta * 10`.
     * @param[in] delta Multiplier input.
     * @return `val + delta * 10`.
     */
    int compute(int delta) const noexcept override { return val + (delta * 10); }
};

/**
 * @brief Accepts and returns `decltype(nullptr)` (`tk_nullptr`) by value.
 * @details Exercises `tk_nullptr` parameter and return lowering (`GCC_JIT_TYPE_VOID_PTR`).
 * @param[in] np Nullptr value parameter.
 * @param[out] out_is_null Pointer receiving `1` if `np == nullptr`.
 * @return `nullptr`.
 */
decltype(nullptr) roundtrip_nullptr(decltype(nullptr) np, int *out_is_null) noexcept {
    int *p = np;
    if (out_is_null) {
        *out_is_null = (p == nullptr) ? 1 : 0;
    }
    return nullptr;
}

/**
 * @brief Checks ASCII substring containment without `<cstring>`.
 * @details Scans `haystack` for `needle`.
 * @param[in] haystack Input string.
 * @param[in] needle Substring to match.
 * @param[out] out_found Pointer receiving `1` if found, `0` otherwise.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t str_contains(const char *haystack, const char *needle, int *out_found) noexcept {
    *out_found = 0;
    if (!haystack || !needle) return GCC_GEN_BE_SUCCESS;
    for (int i = 0; haystack[i] != '\0'; ++i) {
        int j = 0;
        while (needle[j] != '\0' && haystack[i + j] == needle[j]) {
            ++j;
        }
        if (needle[j] == '\0') {
            *out_found = 1;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Runs `nullptr_t`, PMD (`-1` vs offset `0`), PMF, and `std::source_location::current()` checks.
 * @param[out] out_ok Pointer receiving `1` when all checks pass.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED`.
 */
gcc_gen_be_error_t run_types_numerics_checks(int *out_ok) noexcept {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    /* 1. std::nullptr_t */
    int np_ok = 0;
    decltype(nullptr) res_np = roundtrip_nullptr(nullptr, &np_ok);
    int *chk_ptr = res_np;
    ASSERT_EQ(np_ok, 1);
    ASSERT_TRUE(chk_ptr == nullptr);

    /* 2. Null pointer-to-data-member (-1) vs offset-0 first member */
    int PmdHost::*null_pmd = nullptr;
    int PmdHost::*first_pmd = &PmdHost::first_at_zero;
    int PmdHost::*second_pmd = &PmdHost::second_at_four;
    ASSERT_TRUE(null_pmd == nullptr);
    ASSERT_FALSE(first_pmd == nullptr);
    ASSERT_FALSE(first_pmd == null_pmd);
    PmdHost host = { 123, 456 };
    ASSERT_EQ(host.*first_pmd, 123);
    ASSERT_EQ(host.*second_pmd, 456);

    /* 3. Null and virtual pointer-to-member-function */
    int (PmfBase::*null_pmf)(int) const noexcept = nullptr;
    int (PmfBase::*virt_pmf)(int) const noexcept = &PmfBase::compute;
    ASSERT_TRUE(null_pmf == nullptr);
    ASSERT_FALSE(virt_pmf == nullptr);
    PmfDerived der(5);
    const PmfBase *bp = &der;
    ASSERT_EQ((bp->*virt_pmf)(4), 45);

    /* 4. __builtin_FILE, __builtin_LINE, __builtin_FUNCTION & std::source_location::current() */
    const char *b_file = __builtin_FILE();
    unsigned int b_line = (unsigned int)__builtin_LINE();
    const char *b_func = __builtin_FUNCTION();
    std::source_location sl = std::source_location::current();
    int file_ok = 0;
    int func_ok = 0;
    int sl_file_ok = 0;
    GCC_GEN_BE_CHECK(str_contains(b_file, "test_types_numerics_exec.cpp", &file_ok));
    GCC_GEN_BE_CHECK(str_contains(b_func, "run_types_numerics_checks", &func_ok));
    GCC_GEN_BE_CHECK(str_contains(sl.file_name(), "test_types_numerics_exec.cpp", &sl_file_ok));
    ASSERT_EQ(file_ok, 1);
    ASSERT_EQ(func_ok, 1);
    ASSERT_EQ(sl_file_ok, 1);
    ASSERT_TRUE(b_line > 100U && b_line < 200U);
    ASSERT_TRUE(sl.line() > 100U && sl.line() < 200U);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

} // namespace

/**
 * @brief Entry point for `test_types_numerics_exec.cpp`.
 * @details Runs `run_types_numerics_checks` and returns `0` on success.
 * @return `0` on success, `1` on failure.
 */
int main() noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_types_numerics_checks(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok == 1) ? 0 : 1;
}
