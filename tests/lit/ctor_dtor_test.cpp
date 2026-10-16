// RUN: %clang_cc1 -emit-llvm -o - %s | FileCheck %s

struct Base {
    Base() {}
    virtual ~Base() {}
};

struct Derived : virtual Base {
    Derived() {}
    ~Derived() {}
};

// CHECK: define void @_ZN7DerivedC1Ev
// CHECK: call void @_ZN7DerivedC2Ev

// CHECK: define void @_ZN7DerivedD1Ev
// CHECK: call void @_ZN7DerivedD2Ev

// CHECK: define void @_ZN7DerivedD0Ev
// CHECK: call void @_ZN7DerivedD1Ev
// CHECK: call void @_ZdlPv

void test_ctors() {
    Derived d;
}
