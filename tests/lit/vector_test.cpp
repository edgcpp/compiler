// RUN: %clang_cc1 -emit-llvm -o - %s | FileCheck %s

typedef int v4i32 __attribute__((vector_size(16)));

// CHECK: define {{.*}} <4 x i32> @_Z3addDv4_iS_(<4 x i32> %{{.*}}, <4 x i32> %{{.*}})
// CHECK: %{{.*}} = add <4 x i32> %{{.*}}, %{{.*}}
v4i32 add(v4i32 a, v4i32 b) {
    return a + b;
}

// CHECK: define {{.*}} <4 x i32> @_Z3mulDv4_iS_(<4 x i32> %{{.*}}, <4 x i32> %{{.*}})
// CHECK: %{{.*}} = mul <4 x i32> %{{.*}}, %{{.*}}
v4i32 mul(v4i32 a, v4i32 b) {
    return a * b;
}

// CHECK: define {{.*}} <4 x i32> @_Z3cmpDv4_iS_(<4 x i32> %{{.*}}, <4 x i32> %{{.*}})
// CHECK: %{{.*}} = icmp eq <4 x i32> %{{.*}}, %{{.*}}
// CHECK: %{{.*}} = sext <4 x i1> %{{.*}} to <4 x i32>
v4i32 cmp(v4i32 a, v4i32 b) {
    return a == b;
}

// CHECK: define {{.*}} i32 @_Z3subDv4_i(<4 x i32> %{{.*}})
// CHECK: %{{.*}} = extractelement <4 x i32> %{{.*}}, i32 1
int sub(v4i32 a) {
    return a[1];
}
