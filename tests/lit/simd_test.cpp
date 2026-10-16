// RUN: %clang_cc1 -emit-llvm -o - %s | FileCheck %s

typedef float v4f32 __attribute__((vector_size(16)));

// CHECK: define {{.*}} <4 x float> @_Z11test_x86_avxDv4_fS_(<4 x float> %{{.*}}, <4 x float> %{{.*}})
// CHECK: call <4 x float> @llvm.x86.sse.add.ps(<4 x float> %{{.*}}, <4 x float> %{{.*}})
v4f32 test_x86_avx(v4f32 a, v4f32 b) {
    return __builtin_ia32_sse_add_ps(a, b);
}

// CHECK: define {{.*}} <4 x float> @_Z13test_arm_neonDv4_fS_(<4 x float> %{{.*}}, <4 x float> %{{.*}})
// CHECK: call <4 x float> @llvm.aarch64.neon.fmax.v4f32(<4 x float> %{{.*}}, <4 x float> %{{.*}})
v4f32 test_arm_neon(v4f32 a, v4f32 b) {
    return __builtin_neon_fmax_v4f32(a, b);
}
