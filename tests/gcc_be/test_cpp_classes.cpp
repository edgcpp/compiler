/**
 * @file test_cpp_classes.cpp
 * @brief End-to-end C++ class inheritance and base member initialization test.
 * @details Verifies base class constructor delegation and member layout lowering.
 */

#include "test_common.h"

/**
 * @brief Base class containing integer field `x`.
 * @details Initializes `x` via its single-argument constructor.
 */
class Base {
public:
    /** @brief Base integer field `x`. */
    int x;

    /**
     * @brief Constructs `Base` with initial value `x_val`.
     * @details Initializes member `x` to `x_val`.
     * @param[in] x_val Initial value for `x`.
     */
    Base(int x_val) : x(x_val) {}
};

/**
 * @brief Derived class inheriting from `Base` and adding field `y`.
 * @details Delegates `x` initialization to `Base` and initializes `y`.
 */
class Derived : public Base {
public:
    /** @brief Derived integer field `y`. */
    int y;

    /**
     * @brief Constructs `Derived` with `x_val` and `y_val`.
     * @details Initializes `Base(x_val)` and `y(y_val)`.
     * @param[in] x_val Initial value for `Base::x`.
     * @param[in] y_val Initial value for `Derived::y`.
     */
    Derived(int x_val, int y_val) : Base(x_val), y(y_val) {}
};

/**
 * @brief Virtual base class tracking construction and destruction for `C1`/`C2` and `D1`/`D2`/`D0` clone testing.
 * @details Increments `*counter` on construction and adds 10 on destruction.
 */
class VirtBase {
public:
    /** @brief Pointer to counter tracking constructor/destructor invocations. */
    int *counter;

    /**
     * @brief Constructs `VirtBase` and increments `*cnt`.
     * @details Initializes `counter` to `cnt`.
     * @param[in] cnt Pointer to lifecycle counter.
     */
    VirtBase(int *cnt) : counter(cnt) {
        if (counter) *counter += 1;
    }

    /**
     * @brief Virtual destructor for `VirtBase`.
     * @details Adds 10 to `*counter` when destroyed.
     */
    virtual ~VirtBase() {
        if (counter) *counter += 10;
    }
};

/**
 * @brief Derived class virtually inheriting from `VirtBase`.
 * @details Exercises `C1`/`C2` VTT forwarding and virtual `delete base_ptr` via `D0`.
 */
class VirtDerived : virtual public VirtBase {
public:
    /**
     * @brief Constructs `VirtDerived` and adds 2 to `*cnt`.
     * @details Delegates virtual base initialization to `VirtBase(cnt)`.
     * @param[in] cnt Pointer to lifecycle counter.
     */
    VirtDerived(int *cnt) : VirtBase(cnt) {
        if (counter) *counter += 2;
    }

    /**
     * @brief Virtual destructor for `VirtDerived`.
     * @details Adds 20 to `*counter` before `VirtBase::~VirtBase` runs.
     */
    virtual ~VirtDerived() {
        if (counter) *counter += 20;
    }
};

/**
 * @brief Executes C++ class inheritance, VTT forwarding, and virtual deleting destructor (`D0`) checks.
 * @details Instantiates `Derived(1, 2)` and `VirtDerived` and verifies lifecycle counters.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code on failure.
 */
static gcc_gen_be_error_t run_cpp_classes_test(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    Derived d(1, 2);
    ASSERT_EQ(d.x, 1);
    ASSERT_EQ(d.y, 2);

    int lifecycle = 0;
    VirtBase *vb = new VirtDerived(&lifecycle);
    ASSERT_EQ(lifecycle, 3);
    delete vb;
    ASSERT_EQ(lifecycle, 33);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_cpp_classes.cpp`.
 * @details Invokes `run_cpp_classes_test` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_cpp_classes_test(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok) ? 0 : 1;
}
