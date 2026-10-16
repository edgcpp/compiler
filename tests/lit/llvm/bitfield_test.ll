; ModuleID = 'edg_module'
source_filename = "edg_module"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define i32 @read_b(ptr %s) {
entry:
  %s.addr = alloca ptr, align 8
  store ptr %s, ptr %s.addr, align 8
  %0 = load ptr, ptr %s.addr, align 8
  %1 = getelementptr inbounds i8, ptr %0, i64 0
  %2 = load i32, ptr %1, align 4
  %3 = lshr i32 %2, 4
  %4 = and i32 %3, 255
  %5 = shl i32 %4, 24
  %6 = ashr i32 %5, 24
  ret i32 %6
}

define void @write_b(ptr %s, i32 %v) {
entry:
  %s.addr = alloca ptr, align 8
  store ptr %s, ptr %s.addr, align 8
  %v.addr = alloca i32, align 4
  store i32 %v, ptr %v.addr, align 4
  %0 = load ptr, ptr %s.addr, align 8
  %1 = getelementptr inbounds i8, ptr %0, i64 0
  %2 = load i32, ptr %v.addr, align 4
  %3 = load i32, ptr %1, align 4
  %4 = and i32 %2, 255
  %5 = shl i32 %4, 4
  %6 = and i32 %3, -4081
  %7 = or i32 %6, %5
  store i32 %7, ptr %1, align 4
  ret void
}
