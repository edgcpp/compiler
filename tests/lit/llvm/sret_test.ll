; ModuleID = 'edg_module'
source_filename = "edg_module"
target datalayout = "E-m:e-p:64:64-i8:8-i16:16-i32:32-i64:64-f32:32-f64:64-f128:64"
target triple = "x86_64-unknown-linux-gnu"

%0 = type <{ [10 x i64] }>

define %0 @return_large(i32 %a) {
entry:
  %a.addr = alloca i32, align 4
  store i32 %a, ptr %a.addr, align 4
  %s = alloca %0, align 8
  %0 = getelementptr inbounds i8, ptr %s, i64 0
  %1 = load i32, ptr %a.addr, align 4
  %2 = sext i32 %1 to i64
  %3 = load %0, ptr %s, align 1
  ret %0 %3
}

define i32 @main() {
entry:
  %s = alloca %0, align 8
  ret i32 0
}
