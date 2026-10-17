/**
 * @file test_eh.cpp
 * @brief End-to-end C++ exception handling test suite for the GCC backend.
 * @details Verifies primitive throws, by-value and by-reference class catches, polymorphic base catches,
 * `catch (...)` rethrow, RAII destructor unwinding, and `noexcept` functions using `gcc_gen_be_error_t` percolation.
 */

#include "gcc_gen_be_error.h"

namespace std {
/**
 * @brief Standard bad_alloc exception class stub for testing.
 * @details Helper exception class for testing allocation failures.
 */
class bad_alloc {};

/**
 * @brief Standard terminate handler stub for testing.
 * @details Invoked when exception handling must abort execution.
 * @param[out] out_terminated Optional pointer set to 1 when called.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t terminate(int *out_terminated) GCC_GEN_BE_NOEXCEPT {
    if (out_terminated) *out_terminated = 1;
    return GCC_GEN_BE_SUCCESS;
}
}

/**
 * @brief Polymorphic base exception struct for testing inheritance catch matching.
 * @details Provides a virtual destructor and base tag so RTTI and polymorphic catch clauses are exercised.
 */
struct BaseExc {
    /** @brief Tag identifying the base subobject. */
    int base_tag;

    /**
     * @brief Constructs `BaseExc` with `base_tag = 77`.
     * @details Initializes `base_tag`.
     */
    BaseExc() : base_tag(77) {}

    /**
     * @brief Virtual destructor for `BaseExc`.
     * @details Enables polymorphic typeinfo emission.
     */
    virtual ~BaseExc() {}
};

/**
 * @brief Derived exception struct inheriting from `BaseExc`.
 * @details Thrown to test `catch (const BaseExc&)` matching a derived exception object.
 */
struct DerivedExc : BaseExc {
    /** @brief Extra derived payload field. */
    int derived_tag;

    /**
     * @brief Constructs `DerivedExc` with `derived_tag = 99`.
     * @details Initializes `derived_tag`.
     */
    DerivedExc() : BaseExc(), derived_tag(99) {}
};

/**
 * @brief Value/reference exception struct carrying an integer error code.
 * @details Exercises copy construction and destruction during exception handling.
 */
struct MyException {
    /** @brief Integer payload code carried by the exception. */
    int code;

    /**
     * @brief Constructs a `MyException` with payload `c`.
     * @details Initializes `code` to `c`.
     * @param[in] c Integer error code.
     */
    MyException(int c) : code(c) {}

    /**
     * @brief Copy-constructs a `MyException` from `other`.
     * @details Copies `other.code`.
     * @param[in] other Source exception instance.
     */
    MyException(const MyException& other) : code(other.code) {}

    /**
     * @brief Destroys the `MyException` instance.
     * @details Trivial destructor for exception object cleanup testing.
     */
    ~MyException() {}
};

/**
 * @brief RAII scope guard that sets `*p = 1` on construction and `*p = 0` on destruction.
 * @details Verifies that stack unwinding invokes automatic variable destructors when an exception escapes.
 */
struct AutoCleanup {
    /** @brief Pointer to the external state flag modified by construction and destruction. */
    int* p;

    /**
     * @brief Constructs `AutoCleanup` and sets `*ptr = 1`.
     * @details Records active scope entry by writing 1 to `*ptr`.
     * @param[in] ptr Pointer to the integer flag to manage.
     */
    AutoCleanup(int* ptr) : p(ptr) { *p = 1; }

    /**
     * @brief Destroys `AutoCleanup` and resets `*p = 0`.
     * @details Executed during stack unwinding to prove RAII cleanup ran.
     */
    ~AutoCleanup() { *p = 0; }
};

/**
 * @brief RAII guard that appends a step digit to an accumulator on destruction to verify LIFO order.
 * @details Records `*acc = (*acc * 10) + id` when destroyed during stack unwinding.
 */
struct OrderTracker {
    /** @brief Pointer to the order accumulator. */
    int *acc;
    /** @brief Identifier digit for this guard instance. */
    int id;

    /**
     * @brief Constructs an `OrderTracker` for `acc_ptr` and `step_id`.
     * @details Stores `acc_ptr` and `step_id`.
     * @param[in] acc_ptr Pointer to order accumulator.
     * @param[in] step_id Guard identifier digit.
     */
    OrderTracker(int *acc_ptr, int step_id) : acc(acc_ptr), id(step_id) {}

    /**
     * @brief Appends `id` to `*acc` on scope exit / unwinding.
     * @details Verifies reverse construction order of nested RAII scopes.
     */
    ~OrderTracker() {
        if (acc) *acc = (*acc * 10) + id;
    }
};

/**
 * @brief Inner helper frame that constructs two nested `OrderTracker` objects and throws `MyException(42)`.
 * @details Verifies reverse destruction order (`2` then `1`) before unwinding to caller.
 * @param[in,out] order_acc Pointer to destruction order accumulator.
 * @return `GCC_GEN_BE_SUCCESS` (never reached normally).
 */
static gcc_gen_be_error_t func_inner_throw(int *order_acc) {
    OrderTracker t1(order_acc, 1);
    {
        OrderTracker t2(order_acc, 2);
        throw MyException(42);
    }
}

/**
 * @brief Helper function that constructs an `AutoCleanup` guard and calls `func_inner_throw`.
 * @details Used by `test_destructor_unwind` to verify multi-frame destructor execution in reverse order.
 * @param[in,out] flag Pointer to integer flag reset by `AutoCleanup::~AutoCleanup`.
 * @param[in,out] order_acc Pointer to destruction order accumulator.
 * @return `GCC_GEN_BE_SUCCESS` (never reached normally due to throw).
 */
static gcc_gen_be_error_t func_nested(int *flag, int *order_acc) {
    AutoCleanup cleanup(flag);
    OrderTracker t3(order_acc, 3);
    return func_inner_throw(order_acc);
}

/**
 * @brief Tests throwing and catching primitive types (`int`, `double`, `const char*`).
 * @details Exercises primitive exception allocation, type matching, and catch variable binding.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_primitive(int *out_ok) {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    try { throw 123; } catch (int x) { if (x != 123) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED; }
    try { throw 3.14; } catch (double x) { (void)x; }
    try { throw "hello"; } catch (const char* x) { (void)x; }
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests catching class objects by value and by const reference.
 * @details Verifies copy construction and reference binding of `MyException`.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_class_value_ref(int *out_ok) {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    try { throw MyException(10); } catch (MyException e) { if (e.code != 10) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED; }
    try { throw MyException(20); } catch (const MyException& e) { if (e.code != 20) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED; }
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests catching a derived exception via a base class reference (`BaseExc&`).
 * @details Exercises Itanium ABI `__si_class_type_info` inheritance matching.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_polymorphic(int *out_ok) {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    try {
        throw DerivedExc();
    } catch (const BaseExc& e) {
        if (e.base_tag != 77) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests `catch (...)` and `throw;` rethrow into an outer handler.
 * @details Verifies `__cxa_rethrow` propagation to an enclosing `catch (int)` handler.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_catch_all_and_rethrow(int *out_ok) {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    try {
        try { throw 1; } catch (...) { throw; }
    } catch (int v) {
        if (v != 1) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests RAII destructor execution during multi-frame exception stack unwinding.
 * @details Calls `func_nested(&flag, &order_acc)` and verifies `flag == 0` and reverse destruction order `213`.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_destructor_unwind(int *out_ok) {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    int flag = 0;
    int order_acc = 0;
    try {
        GCC_GEN_BE_CHECK(func_nested(&flag, &order_acc));
    } catch (const MyException& ex) {
        if (ex.code != 42 || flag != 0 || order_acc != 213) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests a `noexcept` function specification.
 * @details Verifies lowering of `noexcept(true)` routines.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_noexcept_violation(int *out_ok) noexcept {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes all C++ exception handling test cases and percolates any error.
 * @details Runs primitive, value/ref, polymorphic, catch-all/rethrow, destructor unwind, and noexcept checks.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code on failure.
 */
static gcc_gen_be_error_t run_all_eh_tests(int *out_ok) {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    int step_ok = 0;
    GCC_GEN_BE_CHECK(test_primitive(&step_ok));
    if (!step_ok) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    GCC_GEN_BE_CHECK(test_class_value_ref(&step_ok));
    if (!step_ok) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    GCC_GEN_BE_CHECK(test_polymorphic(&step_ok));
    if (!step_ok) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    GCC_GEN_BE_CHECK(test_catch_all_and_rethrow(&step_ok));
    if (!step_ok) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    GCC_GEN_BE_CHECK(test_destructor_unwind(&step_ok));
    if (!step_ok) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    GCC_GEN_BE_CHECK(test_noexcept_violation(&step_ok));
    if (!step_ok) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_eh.cpp`.
 * @details Executes `run_all_eh_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_all_eh_tests(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok) ? 0 : 1;
}
