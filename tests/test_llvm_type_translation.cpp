/**
 * @file test_llvm_type_translation.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for recursive and cyclic type translation caching.
 * @details Validates primitive, pointer, array, and self-referential struct type translation.
 */

#include "llvm_gen_be_error.h"
#include <iostream>
#include <string>
#include <cassert>
#include <unordered_map>
#include <vector>

using namespace edg;

/**
 * @enum type_kind
 * @brief Mock EDG type kinds used for standalone cyclic type translation tests.
 * @details Represents void, integer, float, pointer, struct, and array types.
 */
enum type_kind {
    tk_void,    ///< Void type kind.
    tk_int,     ///< Integer type kind.
    tk_float,   ///< Floating-point type kind.
    tk_pointer, ///< Pointer type kind.
    tk_struct,  ///< Struct composite type kind.
    tk_array    ///< Array type kind.
};

struct a_type;
typedef a_type* a_type_ptr; ///< Pointer to a mock `a_type` node.

/**
 * @struct a_field
 * @brief Mock EDG struct field node.
 * @details Links a field's type to the next field in a struct's field list.
 */
struct a_field {
    a_type_ptr type; ///< Type of this struct field.
    a_field* next;   ///< Next field in the linked list, or nullptr.
};

/**
 * @struct PointerVariant
 * @brief Pointer type payload containing the pointee type.
 * @details Used inside `a_type::TypeVariant`.
 */
struct PointerVariant {
    a_type_ptr type; ///< Pointee type.
};

/**
 * @struct ArrayVariant
 * @brief Array type payload containing element type and count.
 * @details Used inside `a_type::TypeVariant`.
 */
struct ArrayVariant {
    a_type_ptr elem_type; ///< Element type.
    int elem_count;       ///< Number of elements.
};

/**
 * @struct StructVariant
 * @brief Struct type payload containing the head of the field list.
 * @details Used inside `a_type::TypeVariant`.
 */
struct StructVariant {
    a_field* field_list; ///< Linked list of struct fields.
};

/**
 * @struct TypeVariant
 * @brief Union holding pointer, array, or struct type metadata.
 * @details Discriminated by `a_type::kind`.
 */
union TypeVariant {
    PointerVariant pointer;    ///< Pointer metadata when `kind == tk_pointer`.
    ArrayVariant array;        ///< Array metadata when `kind == tk_array`.
    StructVariant struct_type; ///< Struct metadata when `kind == tk_struct`.
};

/**
 * @struct a_type
 * @brief Mock EDG type node for standalone type caching tests.
 * @details Stores the `type_kind` discriminator and `TypeVariant` union.
 */
struct a_type {
    type_kind kind;      ///< Discriminator indicating the type kind.
    TypeVariant variant; ///< Kind-specific type payload.
};

/**
 * @struct LLVMType
 * @brief Mock LLVM type representation holding a textual type signature.
 * @details Used to verify cyclic struct placeholder insertion and resolution.
 */
struct LLVMType {
    std::string name; ///< Textual representation of the lowered type.
    /**
     * @brief Constructs an `LLVMType` with name @p n.
     * @details Initializes the textual representation of the lowered type.
     * @param[in] n Textual name of the lowered type.
     */
    explicit LLVMType(std::string n) noexcept : name(std::move(n)) {}
};

std::unordered_map<a_type_ptr, LLVMType*> type_cache; ///< Cache mapping mock EDG types to lowered `LLVMType` instances.

/**
 * @brief Translates a mock EDG type into a mock `LLVMType` with cycle detection.
 * @details Inserts a placeholder into `type_cache` before recursing into `tk_struct` fields.
 * @param[in] edg_type Pointer to the mock EDG type node.
 * @param[out] out_type Receives the resulting `LLVMType*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_type` is null.
 */
llvm_gen_be_error_t get_llvm_type(a_type_ptr edg_type, LLVMType** out_type) noexcept {
    if (!out_type) return llvm_gen_be_error_t::invalid_argument;
    if (!edg_type) {
        *out_type = new (std::nothrow) LLVMType("void");
        return *out_type ? llvm_gen_be_error_t::ok : llvm_gen_be_error_t::out_of_memory;
    }
    
    if (type_cache.count(edg_type)) {
        *out_type = type_cache[edg_type];
        return llvm_gen_be_error_t::ok;
    }
    
    if (edg_type->kind == tk_struct) {
        LLVMType* ph = new (std::nothrow) LLVMType("struct.placeholder");
        if (!ph) return llvm_gen_be_error_t::out_of_memory;
        type_cache[edg_type] = ph;
    }
    
    LLVMType* result = nullptr;
    switch (edg_type->kind) {
        case tk_void: result = new (std::nothrow) LLVMType("void"); break;
        case tk_int: result = new (std::nothrow) LLVMType("i32"); break;
        case tk_float: result = new (std::nothrow) LLVMType("float"); break;
        case tk_pointer: {
            LLVMType* pointee = nullptr;
            LLVM_GEN_BE_CHECK(get_llvm_type(edg_type->variant.pointer.type, &pointee));
            result = new (std::nothrow) LLVMType(pointee->name + "*");
            break;
        }
        case tk_array: {
            LLVMType* elem = nullptr;
            LLVM_GEN_BE_CHECK(get_llvm_type(edg_type->variant.array.elem_type, &elem));
            result = new (std::nothrow) LLVMType("[" + std::to_string(edg_type->variant.array.elem_count) + " x " + 
                                                 elem->name + "]");
            break;
        }
        case tk_struct: {
            std::string s = "{ ";
            for (a_field* f = edg_type->variant.struct_type.field_list; f != nullptr; f = f->next) {
                LLVMType* f_ty = nullptr;
                LLVM_GEN_BE_CHECK(get_llvm_type(f->type, &f_ty));
                s += f_ty->name + (f->next ? ", " : "");
            }
            s += " }";
            result = new (std::nothrow) LLVMType(s);
            break;
        }
    }
    if (!result) return llvm_gen_be_error_t::out_of_memory;
    type_cache[edg_type] = result;
    *out_type = result;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests translation of basic integer, float, pointer, and array types.
 * @details Verifies textual representations for `i32`, `float`, `i32*`, and `[10 x i32]`.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t test_basic_types(void) noexcept {
    LLVMType* out = nullptr;
    a_type int_type = { tk_int };
    LLVM_GEN_BE_CHECK(get_llvm_type(&int_type, &out));
    assert(out->name == "i32");
    
    a_type float_type = { tk_float };
    LLVM_GEN_BE_CHECK(get_llvm_type(&float_type, &out));
    assert(out->name == "float");
    
    a_type ptr_type = { tk_pointer };
    ptr_type.variant.pointer.type = &int_type;
    LLVM_GEN_BE_CHECK(get_llvm_type(&ptr_type, &out));
    assert(out->name == "i32*");
    
    a_type arr_type = { tk_array };
    arr_type.variant.array.elem_type = &int_type;
    arr_type.variant.array.elem_count = 10;
    LLVM_GEN_BE_CHECK(get_llvm_type(&arr_type, &out));
    assert(out->name == "[10 x i32]");
    
    std::cout << "test_basic_types passed.\n";
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests translation of self-referential cyclic struct types (`struct Node { int val; Node* next; }`).
 * @details Verifies that `struct.placeholder*` breaks infinite recursion.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t test_cyclic_structures(void) noexcept {
    type_cache.clear();
    
    a_type node_struct = { tk_struct };
    a_type int_type = { tk_int };
    a_type ptr_type = { tk_pointer };
    ptr_type.variant.pointer.type = &node_struct;
    
    a_field next_field = { &ptr_type, nullptr };
    a_field val_field = { &int_type, &next_field };
    node_struct.variant.struct_type.field_list = &val_field;
    
    LLVMType* res = nullptr;
    LLVM_GEN_BE_CHECK(get_llvm_type(&node_struct, &res));
    assert(res->name == "{ i32, struct.placeholder* }");
    
    std::cout << "test_cyclic_structures passed.\n";
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Entry point for the type translation unit test suite.
 * @details Runs `test_basic_types` and `test_cyclic_structures`.
 * @return 0 on success, or non-zero on failure.
 */
int main(void) {
    if (test_basic_types() != llvm_gen_be_error_t::ok) return 1;
    if (test_cyclic_structures() != llvm_gen_be_error_t::ok) return 1;
    std::cout << "All Type Translation tests passed!\n";
    return 0;
}
