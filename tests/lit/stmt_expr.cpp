// RUN: %clang_cc1 -emit-llvm -o - %s | FileCheck %s

int test_stmt_expr() {
    // CHECK: %stmt_expr_res = alloca i32
    // CHECK: store i32 5, ptr %stmt_expr_res
    // CHECK: %{{.*}} = load i32, ptr %stmt_expr_res
    return ({ int x = 5; x; });
}
