// RUN: %clang_cc1 -emit-llvm -o - %s | FileCheck %s

// CHECK: define {{.*}} i32 @_Z10test_atomicPi(ptr noundef %{{.*}})
// CHECK: %{{.*}} = load atomic i32, ptr %{{.*}} seq_cst, align 4
int test_atomic(int* p) {
    return __atomic_load_n(p, __ATOMIC_SEQ_CST);
}

// CHECK: define {{.*}} void @_Z11test_atomicPii(ptr noundef %{{.*}}, i32 noundef %{{.*}})
// CHECK: store atomic i32 %{{.*}}, ptr %{{.*}} release, align 4
void test_atomic(int* p, int v) {
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
}

// CHECK: define {{.*}} i32 @_Z16test_atomic_xchgPii(ptr noundef %{{.*}}, i32 noundef %{{.*}})
// CHECK: %{{.*}} = atomicrmw xchg ptr %{{.*}}, i32 %{{.*}} acquire
int test_atomic_xchg(int* p, int v) {
    return __atomic_exchange_n(p, v, __ATOMIC_ACQUIRE);
}

// CHECK: define {{.*}} i32 @_Z15test_atomic_addPii(ptr noundef %{{.*}}, i32 noundef %{{.*}})
// CHECK: %{{.*}} = atomicrmw add ptr %{{.*}}, i32 %{{.*}} monotonic
int test_atomic_add(int* p, int v) {
    return __atomic_fetch_add(p, v, __ATOMIC_RELAXED);
}

// CHECK: define {{.*}} i1 @_Z16test_atomic_cmpxPii(ptr noundef %{{.*}}, ptr noundef %{{.*}}, i32 noundef %{{.*}})
// CHECK: %{{.*}} = cmpxchg ptr %{{.*}}, i32 %{{.*}}, i32 %{{.*}} seq_cst seq_cst
bool test_atomic_cmpx(int* p, int* exp, int v) {
    return __atomic_compare_exchange_n(p, exp, v, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

// CHECK: define {{.*}} void @_Z11test_fencesv()
// CHECK: fence seq_cst
void test_fences() {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}
