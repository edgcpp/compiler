; ModuleID = 'edg_module'
source_filename = "edg_module"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%0 = type <{ [10 x i64] }>

define void @copy_struct(ptr %dest, ptr %src) {
entry:
  %dest.addr = alloca ptr, align 8
  store ptr %dest, ptr %dest.addr, align 8
  %src.addr = alloca ptr, align 8
  store ptr %src, ptr %src.addr, align 8
  %0 = load ptr, ptr %dest.addr, align 8
  %1 = load ptr, ptr %src.addr, align 8
  %2 = load %0, ptr %1, align 1
  call void @llvm.memcpy.p0.p0.i64(ptr align 1 %0, ptr align 1 %1, i64 80, i1 false)
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly captures(none), ptr noalias readonly captures(none), i64, i1 immarg) #0

attributes #0 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
