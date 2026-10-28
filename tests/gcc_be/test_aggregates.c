/**
 * @file test_aggregates.c
 * @brief End-to-end C test for nested structs, bitfields, unions, arrays, and relocatable static initializers.
 * @details Verifies aggregate static initialization (`ck_aggregate`), designated union initializers,
 * bitfield packing, and static pointer relocations (`&int_arr[1][1]`, `&global_struct.nested[1].y`).
 */

#include "test_common.h"

/** @brief MyBitfield struct. */
struct MyBitfield {
    /** @brief a member. */
    unsigned int a : 3;
    /** @brief b member. */
    unsigned int b : 5;
    /** @brief c member. */
    unsigned int c : 24;
};

/** @brief Nested struct. */
struct Nested {
    /** @brief x member. */
    int x;
    /** @brief y member. */
    int y;
};

/** @brief MyStruct struct. */
struct MyStruct {
    /** @brief nested member. */
    struct Nested nested[2];
    /** @brief bf member. */
    struct MyBitfield bf;
    /** @brief the union */
    union {
        /** @brief i member. */
        int i;
        /** @brief f member. */
        float f;
    } u; /**< @brief u member */
};

/** @brief global_struct member. */
struct MyStruct global_struct = {
    { {1, 2}, {3, 4} },
    { 1, 2, 3 },
    { .i = 42 }
};

/** @brief str_arr member. */
char str_arr[10] = "hello";
/** @brief int_arr member. */
int int_arr[2][3] = { {1, 2, 3}, {4, 5, 6} };
/** @brief Relocatable static pointer initialized to `&int_arr[1][1]`. */
int *reloc_arr_elem_ptr = &int_arr[1][1];
/** @brief Relocatable static pointer initialized to `&global_struct.nested[1].y`. */
int *reloc_struct_field_ptr = &global_struct.nested[1].y;

/**
 * @brief Executes aggregate, bitfield, union, and relocatable address+offset initializer assertions.
 * @details Verifies static initialization of nested structs, bitfields, unions, arrays, and pointer relocations.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code on failure.
 */
static gcc_gen_be_error_t run_aggregates_test(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    ASSERT_EQ(global_struct.nested[1].y, 4);
    ASSERT_EQ(global_struct.bf.a, 1U);
    ASSERT_EQ(global_struct.u.i, 42);
    ASSERT_EQ(str_arr[0], 'h');
    ASSERT_EQ(str_arr[5], '\0');
    ASSERT_EQ(int_arr[1][2], 6);
    ASSERT_TRUE(reloc_arr_elem_ptr != 0);
    ASSERT_EQ(*reloc_arr_elem_ptr, 5);
    ASSERT_TRUE(reloc_struct_field_ptr != 0);
    ASSERT_EQ(*reloc_struct_field_ptr, 4);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point.
 * @details Invokes `run_aggregates_test` and returns 0 on success.
 * @return 0 on success.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_aggregates_test(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok) ? 0 : 1;
}
