// RUN: %clang_cc1 -emit-llvm -o - %s | FileCheck %s

// CHECK: define {{.*}} i1 @_Z13test_overflowiiPi(i32 noundef %{{.*}}, i32 noundef %{{.*}}, ptr noundef %{{.*}})
// CHECK: %{{.*}} = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %{{.*}}, i32 %{{.*}})
// CHECK: %{{.*}} = extractvalue { i32, i1 } %{{.*}}, 0
// CHECK: store i32 %{{.*}}, ptr %{{.*}}
// CHECK: %{{.*}} = extractvalue { i32, i1 } %{{.*}}, 1
bool test_overflow(int a, int b, int* res) {
    return __builtin_sadd_overflow(a, b, res);
}

// CHECK: define {{.*}} i32 @_Z8test_clzj(i32 noundef %{{.*}})
// CHECK: %{{.*}} = call i32 @llvm.ctlz.i32(i32 %{{.*}}, i1 true)
int test_clz(unsigned int x) {
    return __builtin_clz(x);
}

// CHECK: define {{.*}} i32 @_Z11test_expectii(i32 noundef %{{.*}}, i32 noundef %{{.*}})
// CHECK: %{{.*}} = call i64 @llvm.expect.i64(i64 %{{.*}}, i64 %{{.*}})
int test_expect(int a, int b) {
    return __builtin_expect(a, b);
}

// CHECK: define {{.*}} i1 @_Z17test_is_constant_pi(ptr noundef %{{.*}})
// CHECK: %{{.*}} = call i1 @llvm.is.constant.p0(ptr %{{.*}})
bool test_is_constant_p(int* p) {
    return __builtin_constant_p(p);
}
