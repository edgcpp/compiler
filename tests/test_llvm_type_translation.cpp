#include <iostream>
#include <string>
#include <cassert>
#include <unordered_map>
#include <vector>

// Mock EDG Type Enums
enum type_kind {
    tk_void, tk_int, tk_float, tk_pointer, tk_struct, tk_array
};

struct a_type;
typedef a_type* a_type_ptr;

struct a_field {
    a_type_ptr type;
    a_field* next;
};

struct a_type {
    type_kind kind;
    union {
        struct { a_type_ptr type; } pointer;
        struct { a_type_ptr elem_type; int elem_count; } array;
        struct { a_field* field_list; } struct_type;
    } variant;
};

// Mock LLVM Types
struct LLVMType {
    std::string name;
    LLVMType(std::string n) : name(n) {}
};

std::unordered_map<a_type_ptr, LLVMType*> type_cache;

LLVMType* get_llvm_type(a_type_ptr edg_type) {
    if (!edg_type) return new LLVMType("void");
    
    if (type_cache.count(edg_type)) {
        return type_cache[edg_type];
    }
    
    // To prevent infinite recursion on cyclic structs, we insert a placeholder
    if (edg_type->kind == tk_struct) {
        type_cache[edg_type] = new LLVMType("struct.placeholder");
    }
    
    LLVMType* result = nullptr;
    switch (edg_type->kind) {
        case tk_void: result = new LLVMType("void"); break;
        case tk_int: result = new LLVMType("i32"); break;
        case tk_float: result = new LLVMType("float"); break;
        case tk_pointer: 
            result = new LLVMType(get_llvm_type(edg_type->variant.pointer.type)->name + "*"); 
            break;
        case tk_array:
            result = new LLVMType("[" + std::to_string(edg_type->variant.array.elem_count) + " x " + 
                                  get_llvm_type(edg_type->variant.array.elem_type)->name + "]");
            break;
        case tk_struct: {
            std::string s = "{ ";
            for (a_field* f = edg_type->variant.struct_type.field_list; f != nullptr; f = f->next) {
                s += get_llvm_type(f->type)->name + (f->next ? ", " : "");
            }
            s += " }";
            result = new LLVMType(s);
            break;
        }
    }
    type_cache[edg_type] = result;
    return result;
}

void test_basic_types() {
    a_type int_type = { tk_int };
    assert(get_llvm_type(&int_type)->name == "i32");
    
    a_type float_type = { tk_float };
    assert(get_llvm_type(&float_type)->name == "float");
    
    a_type ptr_type = { tk_pointer };
    ptr_type.variant.pointer.type = &int_type;
    assert(get_llvm_type(&ptr_type)->name == "i32*");
    
    a_type arr_type = { tk_array };
    arr_type.variant.array.elem_type = &int_type;
    arr_type.variant.array.elem_count = 10;
    assert(get_llvm_type(&arr_type)->name == "[10 x i32]");
    
    std::cout << "test_basic_types passed.\n";
}

void test_cyclic_structures() {
    type_cache.clear();
    
    // struct Node { int val; Node* next; }
    a_type node_struct = { tk_struct };
    a_type int_type = { tk_int };
    a_type ptr_type = { tk_pointer };
    ptr_type.variant.pointer.type = &node_struct;
    
    a_field next_field = { &ptr_type, nullptr };
    a_field val_field = { &int_type, &next_field };
    node_struct.variant.struct_type.field_list = &val_field;
    
    LLVMType* res = get_llvm_type(&node_struct);
    assert(res->name == "{ i32, struct.placeholder* }");
    
    std::cout << "test_cyclic_structures passed.\n";
}

int main() {
    test_basic_types();
    test_cyclic_structures();
    std::cout << "All Type Translation tests passed!\n";
    return 0;
}
