// RUN: %clang_cc1 -std=c++20 -emit-llvm -o - %s | FileCheck %s

#include <coroutine>

struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};

// CHECK: define {{.*}} @_Z4corov()
// CHECK: call token @llvm.coro.id
// CHECK: call i1 @llvm.coro.alloc
// CHECK: call i64 @llvm.coro.size
// CHECK: call ptr @_Znwm
// CHECK: call ptr @llvm.coro.begin
// CHECK: call i8 @llvm.coro.suspend
// CHECK: call ptr @llvm.coro.free
// CHECK: call i1 @llvm.coro.end

Task coro() {
    co_return;
}
