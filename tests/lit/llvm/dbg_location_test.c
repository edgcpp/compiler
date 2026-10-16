// RUN: cpfe --c99 --edg_base_dir include_c++ --gen_llvm_bc_file_name %t.bc %s
// RUN: llvm-dis %t.bc -o - | FileCheck %s

int test_dbg_location(int a, int b) {
    int c = a + b;
    c = c * 2;
    return c;
}

// CHECK: define{{.*}} i32 @test_dbg_location
// CHECK: call void @llvm.dbg.declare
// CHECK: call void @llvm.dbg.value
// CHECK: ret i32
