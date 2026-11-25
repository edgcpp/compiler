// RUN: cpfe --c99 --edg_base_dir include_c++ --gen_llvm_file_name %t.ll %s
// RUN: cat %t.ll | FileCheck %s

struct LargeStruct {
    long long data[10];
};

// CHECK: define void @return_large(ptr {{.*}} sret
struct LargeStruct return_large(int a) {
    struct LargeStruct s;
    s.data[0] = a;
    return s;
}

int main() {
    // CHECK: call void @return_large(ptr {{.*}} sret
    struct LargeStruct s = return_large(42);
    return 0;
}
