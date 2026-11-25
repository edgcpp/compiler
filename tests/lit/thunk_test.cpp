// RUN: %clang_cc1 -emit-llvm -o - %s | FileCheck %s

struct A {
    virtual void func() {}
};

struct B {
    virtual void func() {}
};

struct C : A, B {
    virtual void func() override {}
};

// CHECK: define linkonce_odr void @_ZTv0_n24_N1C4funcEv
// CHECK: %{{.*}} = load ptr, ptr %this
// CHECK: %{{.*}} = getelementptr inbounds ptr, ptr %{{.*}}, i64 -3
// CHECK: %{{.*}} = load i64, ptr %{{.*}}
// CHECK: %{{.*}} = getelementptr inbounds i8, ptr %this, i64 %{{.*}}
// CHECK: tail call void @_ZN1C4funcEv(ptr noundef nonnull align 8 dereferenceable(8) %{{.*}})
// CHECK: ret void

void test_thunk() {
    C c;
    B* b = &c;
    b->func();
}
