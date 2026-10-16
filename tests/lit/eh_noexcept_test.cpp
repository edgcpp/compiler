// RUN: %clang_cc1 -emit-llvm -fcxx-exceptions -fexceptions -o - %s | FileCheck %s

// CHECK: define {{.*}} void @_Z4testv()
// CHECK: %{{.*}} = landingpad
// CHECK-NEXT: catch ptr null
// CHECK: call void @__clang_call_terminate

void test() noexcept {
    throw 1;
}
