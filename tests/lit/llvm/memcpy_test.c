// RUN: cpfe --c99 --edg_base_dir include_c++ --gen_llvm_file_name %t.ll %s
// RUN: cat %t.ll | FileCheck %s

struct LargeStruct {
    long long data[10];
};

void copy_struct(struct LargeStruct* dest, struct LargeStruct* src) {
    // CHECK: call void @llvm.memcpy
    *dest = *src;
}
