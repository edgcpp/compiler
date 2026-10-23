/**
 * @file test_virtual.cpp
 * @brief End-to-end C++ virtual dispatch, multiple inheritance, and virtual base test suite.
 * @details Exercises Itanium ABI vtable dispatch, `this`-adjustment thunks, `vcall` offset thunks,
 * and virtual base subobject dispatch using `gcc_gen_be_error_t` out-parameter methods.
 */

#include "test_common.h"

/**
 * @brief Primary polymorphic base class.
 * @details Defines virtual `get1` returning `gcc_gen_be_error_t` with out-parameter `out_val`.
 */
class Base1 {
public:
    /**
     * @brief Retrieves the `Base1` value into `out_val`.
     * @details Writes 10 to `*out_val`.
     * @param[out] out_val Pointer to receive the integer value.
     * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
     */
    virtual gcc_gen_be_error_t get1(int *out_val) {
        if (!out_val) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
        *out_val = 10;
        return GCC_GEN_BE_SUCCESS;
    }

    /**
     * @brief Virtual destructor for `Base1`.
     * @details Ensures proper polymorphic cleanup.
     */
    virtual ~Base1() {}
};

/**
 * @brief Secondary polymorphic base class for multiple-inheritance testing.
 * @details Defines virtual `get2` returning `gcc_gen_be_error_t` with out-parameter `out_val`.
 */
class Base2 {
public:
    /**
     * @brief Retrieves the `Base2` value into `out_val`.
     * @details Writes 20 to `*out_val`.
     * @param[out] out_val Pointer to receive the integer value.
     * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
     */
    virtual gcc_gen_be_error_t get2(int *out_val) {
        if (!out_val) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
        *out_val = 20;
        return GCC_GEN_BE_SUCCESS;
    }

    /**
     * @brief Virtual destructor for `Base2`.
     * @details Ensures proper polymorphic cleanup.
     */
    virtual ~Base2() {}
};

/**
 * @brief Multiple-inheritance derived class overriding both `Base1::get1` and `Base2::get2`.
 * @details Exercises non-virtual `this`-adjustment thunk emission for `Base2`.
 */
class DerivedMI : public Base1, public Base2 {
public:
    /**
     * @brief Overrides `Base1::get1` and writes 11 to `out_val`.
     * @details Validates `out_val` and stores 11.
     * @param[out] out_val Pointer to receive the integer value.
     * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
     */
    gcc_gen_be_error_t get1(int *out_val) override {
        if (!out_val) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
        *out_val = 11;
        return GCC_GEN_BE_SUCCESS;
    }

    /**
     * @brief Overrides `Base2::get2` and writes 22 to `out_val`.
     * @details Dispatched via `Base2` secondary vtable with `this`-pointer adjustment.
     * @param[out] out_val Pointer to receive the integer value.
     * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
     */
    gcc_gen_be_error_t get2(int *out_val) override {
        if (!out_val) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
        *out_val = 22;
        return GCC_GEN_BE_SUCCESS;
    }
};

/**
 * @brief Virtual base class for testing `vcall` offset adjustment thunks.
 * @details Defines virtual `vget` returning `gcc_gen_be_error_t`.
 */
class VBase {
public:
    /**
     * @brief Retrieves the `VBase` value into `out_val`.
     * @details Writes 30 to `*out_val`.
     * @param[out] out_val Pointer to receive the integer value.
     * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
     */
    virtual gcc_gen_be_error_t vget(int *out_val) {
        if (!out_val) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
        *out_val = 30;
        return GCC_GEN_BE_SUCCESS;
    }
};

/**
 * @brief Derived class virtually inheriting from `VBase`.
 * @details Overrides `VBase::vget` to test virtual base `vcall` offset thunks.
 */
class VDerived : virtual public VBase {
public:
    /**
     * @brief Overrides `VBase::vget` and writes 33 to `out_val`.
     * @details Dispatched through virtual base vtable with `vcall` adjustment.
     * @param[out] out_val Pointer to receive the integer value.
     * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
     */
    gcc_gen_be_error_t vget(int *out_val) override {
        if (!out_val) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
        *out_val = 33;
        return GCC_GEN_BE_SUCCESS;
    }
};

/**
 * @brief Base factory class for testing polymorphic object allocation.
 * @details Defines virtual `get_ptr` writing a `Base1 *` to `out_ptr`.
 */
class RetBase {
public:
    /**
     * @brief Allocates or returns a `Base1` pointer via `out_ptr`.
     * @details Sets `*out_ptr = 0` in the base implementation.
     * @param[out] out_ptr Pointer to receive the `Base1 *` instance.
     * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
     */
    virtual gcc_gen_be_error_t get_ptr(Base1 **out_ptr) {
        if (!out_ptr) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
        *out_ptr = 0;
        return GCC_GEN_BE_SUCCESS;
    }
};

/**
 * @brief Derived factory class overriding `RetBase::get_ptr`.
 * @details Allocates a `DerivedMI` instance and stores its `Base1 *` address in `out_ptr`.
 */
class RetDerived : public RetBase {
public:
    /**
     * @brief Allocates a new `DerivedMI` and stores it in `out_ptr`.
     * @details Exercises polymorphic allocation and virtual dispatch.
     * @param[out] out_ptr Pointer to receive the newly allocated `Base1 *`.
     * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
     */
    gcc_gen_be_error_t get_ptr(Base1 **out_ptr) override {
        if (!out_ptr) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
        *out_ptr = new DerivedMI();
        return GCC_GEN_BE_SUCCESS;
    }
};

/**
 * @brief Runs multiple-inheritance, virtual-base, and factory dispatch checks.
 * @details Percolates all virtual method calls via `GCC_GEN_BE_CHECK` and `ASSERT_EQ`.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code on failure.
 */
static gcc_gen_be_error_t run_virtual_test(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    DerivedMI dmi;
    Base1* b1 = &dmi;
    Base2* b2 = &dmi;

    int v1 = 0;
    GCC_GEN_BE_CHECK(b1->get1(&v1));
    ASSERT_EQ(v1, 11);
    int v2 = 0;
    GCC_GEN_BE_CHECK(b2->get2(&v2));
    ASSERT_EQ(v2, 22);

    VDerived vd;
    VBase* vb = &vd;
    int v3 = 0;
    GCC_GEN_BE_CHECK(vb->vget(&v3));
    ASSERT_EQ(v3, 33);

    RetDerived rd;
    RetBase* rb = &rd;
    Base1* ptr = 0;
    GCC_GEN_BE_CHECK(rb->get_ptr(&ptr));
    ASSERT_TRUE(ptr != 0);
    int v4 = 0;
    GCC_GEN_BE_CHECK(ptr->get1(&v4));
    ASSERT_EQ(v4, 11);
    delete ptr;

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_virtual.cpp`.
 * @details Invokes `run_virtual_test` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_virtual_test(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok) ? 0 : 1;
}
