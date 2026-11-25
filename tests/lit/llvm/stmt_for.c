// RUN: clang -S -emit-llvm %s -o - | FileCheck %s

void test_for() {
    // CHECK: for.cond
    // CHECK: for.body
    // CHECK: for.inc
    // CHECK: for.end
    for (int i = 0; i < 10; ++i) {
        if (i == 5) break;
        if (i == 2) continue;
    }
}