// RUN: clang -S -emit-llvm %s -o - | FileCheck %s

struct X {
    int a;
    float b;
};

float test_opaque(struct X* ptr) {
    // CHECK: getelementptr inbounds %struct.X, ptr %ptr, i32 0, i32 1
    return ptr->b;
}