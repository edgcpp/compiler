// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -emit-llvm -fcxx-exceptions -fexceptions -fms-extensions -o - %s | FileCheck %s

// CHECK: define dso_local void @"?test_msvc_eh@@YAXXZ"()
// CHECK: %{{.*}} = catchswitch within none [label %{{.*}}] unwind to caller
// CHECK: %{{.*}} = catchpad within %{{.*}}
// CHECK: catchret from %{{.*}} to label %{{.*}}
// CHECK: %{{.*}} = cleanuppad within none []
// CHECK: cleanupret from %{{.*}} unwind to caller

void test_msvc_eh() {
    __try {
        
    } __except(1) {
        
    }
    
    __try {
        
    } __finally {
        
    }
}
