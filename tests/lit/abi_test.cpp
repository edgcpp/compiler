// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s --check-prefix=X86
// RUN: %clang_cc1 -triple armv7-unknown-linux-gnueabihf -emit-llvm -o - %s | FileCheck %s --check-prefix=ARM
// RUN: %clang_cc1 -triple wasm32-unknown-unknown -emit-llvm -o - %s | FileCheck %s --check-prefix=WASM

struct Big {
    long long a, b, c;
};

// X86: define {{.*}} void @_Z4test3Big(ptr {{.*}} %b)
// ARM: define {{.*}} void @_Z4test3Big(ptr {{.*}} %b)
// WASM: define {{.*}} void @_Z4test3Big(ptr {{.*}} %b)
void test(Big b) {
}
