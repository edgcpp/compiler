// RUN: %clang_cc1 -emit-llvm -o - %s | FileCheck %s

struct A {
    virtual void vfunc() {}
    void nvfunc() {}
};

// CHECK: define void @_Z12test_vptrmemv
// CHECK: %{{.*}} = load { ptr, i64 }, ptr %{{.*}}
// CHECK: %{{.*}} = extractvalue { ptr, i64 } %{{.*}}, 0
// CHECK: %{{.*}} = extractvalue { ptr, i64 } %{{.*}}, 1
// CHECK: %{{.*}} = getelementptr inbounds i8, ptr %{{.*}}, i64 %{{.*}}
// CHECK: %{{.*}} = trunc i64 %{{.*}} to i1
// CHECK: br i1 %{{.*}}, label %{{.*}}, label %{{.*}}
// CHECK: memptr.vcall:
// CHECK: memptr.nvcall:

void test_vptrmem() {
    A a;
    void (A::*pmf)() = &A::vfunc;
    (a.*pmf)();
}
