// RUN: %clang_cc1 -emit-llvm -fcxx-exceptions -fexceptions -o - %s | FileCheck %s

struct A {
    ~A() {}
};

// CHECK: define {{.*}} void @_Z4testv()
// CHECK: invoke void @_Z5throwv()
// CHECK-NEXT: to label %{{.*}} unwind label %[[LPAD:.*]]

// CHECK: [[LPAD]]:
// CHECK: %{{.*}} = landingpad
// CHECK-NEXT: cleanup
// CHECK: call void @_ZN1AD1Ev(ptr noundef nonnull align 1 dereferenceable(1) %{{.*}})

void throwv() { throw 1; }

void test() {
    A a;
    throwv();
}
