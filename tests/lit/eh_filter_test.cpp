// RUN: %clang_cc1 -emit-llvm -fcxx-exceptions -fexceptions -o - %s | FileCheck %s

// CHECK: define {{.*}} void @_Z4testv()
// CHECK: %{{.*}} = landingpad
// CHECK-NEXT: filter [1 x ptr] [ptr @_ZTIi]
// CHECK: call void @__cxa_call_unexpected

void test() throw(int) {
    throw 1.0;
}
