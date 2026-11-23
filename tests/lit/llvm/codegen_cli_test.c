// RUN: cpfe --c99 --edg_base_dir include_c++ --gen_llvm_bc_file_name %t.bc --gen_obj_file_name %t.o --gen_asm_file_name %t.s %s
// RUN: llvm-bcanalyzer %t.bc | FileCheck %s -check-prefix=CHECK-BC
// RUN: file %t.o | FileCheck %s -check-prefix=CHECK-OBJ
// RUN: cat %t.s | FileCheck %s -check-prefix=CHECK-ASM

int main() {
    return 42;
}

// CHECK-BC: Bitcode Analysis Of Module
// CHECK-OBJ: relocatable
// CHECK-ASM: .text
// CHECK-ASM: main:
