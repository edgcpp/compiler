// RUN: %edgcpp -S -emit-llvm %s -o - | FileCheck %s

// CHECK: @x = thread_local(initialexec) global i32 0
// CHECK: @y = thread_local(localdynamic) global i32 0
// CHECK: @z = thread_local global i32 0
// CHECK: @w = thread_local(localexec) global i32 0

__thread int x __attribute__((tls_model("initial-exec")));
__thread int y __attribute__((tls_model("local-dynamic")));
__thread int z __attribute__((tls_model("global-dynamic")));
__thread int w __attribute__((tls_model("local-exec")));
