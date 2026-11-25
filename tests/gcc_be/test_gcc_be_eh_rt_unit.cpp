/**
 * @file test_gcc_be_eh_rt_unit.cpp
 * @brief Comprehensive unit test for `lib_src/gcc_be_eh_rt.c` DWARF unwind bridge and `_ZTI` matcher.
 * @details Exercises Itanium C++ ABI `_ZTI` matching across leaf `__class_type_info`,
 * `__si_class_type_info`, `__vmi_class_type_info` (`flags == 0, 1, 2, 3` and `base_count > 16`),
 * `__pointer_type_info` CV qualification conversions, `[cv] void*` vs function-pointer rejection,
 * `nullptr` -> pointer catch, thread-local catch-stack push/pop/overflow, nested exception replacement,
 * unhandled rethrow terminate hook, `_Unwind_Backtrace` personality edge cases, and OOM sweeps.
 * @ingroup gcc_gen_be_tests
 */

#include "gcc_be_eh_rt.h"
#include "test_common.h"
#include <cxxabi.h>
#include <csetjmp>
#include <cstdint>
#include <exception>
#include <pthread.h>
#include <unwind.h>

extern "C" {
/** @brief System Itanium ABI `__class_type_info` vtable. */
extern const void *_ZTVN10__cxxabiv117__class_type_infoE[];
/** @brief System Itanium ABI `__si_class_type_info` vtable. */
extern const void *_ZTVN10__cxxabiv120__si_class_type_infoE[];
/** @brief System Itanium ABI `__vmi_class_type_info` vtable. */
extern const void *_ZTVN10__cxxabiv121__vmi_class_type_infoE[];
/** @brief System Itanium ABI `__pointer_type_info` vtable. */
extern const void *_ZTVN10__cxxabiv119__pointer_type_infoE[];

/**
 * @brief Assembly helper with an LSDA range covering only the middle call so calls 0 and 2 test `ip <= start_pc` and `ip > end_pc`.
 * @param[in] pre_fn Callback invoked before LSDA `start_pc`.
 * @param[in] mid_fn Callback invoked inside `[start_pc, end_pc]`.
 * @param[in] post_fn Callback invoked after LSDA `end_pc`.
 * @return Always `GCC_GEN_BE_SUCCESS` (`0`).
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t test_eh_rt_lsda_window_probe(
    edg_gcc_eh_region_fn_t pre_fn,
    edg_gcc_eh_region_fn_t mid_fn,
    edg_gcc_eh_region_fn_t post_fn) GCC_GEN_BE_NOEXCEPT;
}

#if defined(__APPLE__) && defined(__aarch64__)
__asm__(R"edg_test_asm(
  .text
  .globl _test_eh_rt_lsda_window_probe
  .p2align 2
_test_eh_rt_lsda_window_probe:
  .cfi_startproc
  .cfi_personality 155, ___gcc_be_eh_personality_v0
  .cfi_lsda 16, Ltest_probe_lsda
  stp x22, x21, [sp, #-48]!
  stp x20, x19, [sp, #16]
  stp x29, x30, [sp, #32]
  add x29, sp, #32
  .cfi_def_cfa w29, 16
  .cfi_offset w30, -8
  .cfi_offset w29, -16
  .cfi_offset w19, -24
  .cfi_offset w20, -32
  .cfi_offset w21, -40
  .cfi_offset w22, -48
  mov x19, x0
  mov x20, x1
  mov x21, x2
  mov x0, #0
  blr x19
Ltest_probe_start:
  mov x0, #0
  blr x20
Ltest_probe_end:
  mov x0, #0
  blr x21
Ltest_probe_lp:
  mov w0, #0
  ldp x29, x30, [sp, #32]
  ldp x20, x19, [sp, #16]
  ldp x22, x21, [sp], #48
  ret
  .cfi_endproc
  .section __TEXT,__gcc_except_tab
  .p2align 2
Ltest_probe_lsda:
  .long Ltest_probe_start - _test_eh_rt_lsda_window_probe
  .long Ltest_probe_end - _test_eh_rt_lsda_window_probe
  .long Ltest_probe_lp - _test_eh_rt_lsda_window_probe
  .text
)edg_test_asm");
#elif defined(__APPLE__) && defined(__x86_64__)
__asm__(R"edg_test_asm(
  .text
  .globl _test_eh_rt_lsda_window_probe
  .p2align 4
_test_eh_rt_lsda_window_probe:
  .cfi_startproc
  .cfi_personality 155, ___gcc_be_eh_personality_v0
  .cfi_lsda 16, Ltest_probe_lsda
  pushq %rbp
  .cfi_def_cfa_offset 16
  .cfi_offset %rbp, -16
  movq %rsp, %rbp
  .cfi_def_cfa_register %rbp
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %rax
  .cfi_offset %r12, -24
  .cfi_offset %r13, -32
  .cfi_offset %r14, -40
  movq %rdi, %r12
  movq %rsi, %r13
  movq %rdx, %r14
  xorl %edi, %edi
  callq *%r12
Ltest_probe_start:
  xorl %edi, %edi
  callq *%r13
Ltest_probe_end:
  xorl %edi, %edi
  callq *%r14
Ltest_probe_lp:
  xorl %eax, %eax
  addq $8, %rsp
  popq %r14
  popq %r13
  popq %r12
  popq %rbp
  retq
  .cfi_endproc
  .section __TEXT,__gcc_except_tab
  .p2align 2
Ltest_probe_lsda:
  .long Ltest_probe_start - _test_eh_rt_lsda_window_probe
  .long Ltest_probe_end - _test_eh_rt_lsda_window_probe
  .long Ltest_probe_lp - _test_eh_rt_lsda_window_probe
  .text
)edg_test_asm");
#elif defined(__aarch64__)
__asm__(R"edg_test_asm(
  .text
  .globl test_eh_rt_lsda_window_probe
  .p2align 2
test_eh_rt_lsda_window_probe:
  .cfi_startproc
  .cfi_personality 0x9b, DW.ref.__gcc_be_eh_personality_v0
  .cfi_lsda 0x1b, .Ltest_probe_lsda
  stp x22, x21, [sp, #-48]!
  stp x20, x19, [sp, #16]
  stp x29, x30, [sp, #32]
  add x29, sp, #32
  .cfi_def_cfa w29, 16
  .cfi_offset w30, -8
  .cfi_offset w29, -16
  .cfi_offset w19, -24
  .cfi_offset w20, -32
  .cfi_offset w21, -40
  .cfi_offset w22, -48
  mov x19, x0
  mov x20, x1
  mov x21, x2
  mov x0, #0
  blr x19
.Ltest_probe_start:
  mov x0, #0
  blr x20
.Ltest_probe_end:
  mov x0, #0
  blr x21
.Ltest_probe_lp:
  mov w0, #0
  ldp x29, x30, [sp, #32]
  ldp x20, x19, [sp, #16]
  ldp x22, x21, [sp], #48
  ret
  .cfi_endproc
  .section .gcc_except_table,"a",@progbits
  .p2align 2
.Ltest_probe_lsda:
  .long .Ltest_probe_start - test_eh_rt_lsda_window_probe
  .long .Ltest_probe_end - test_eh_rt_lsda_window_probe
  .long .Ltest_probe_lp - test_eh_rt_lsda_window_probe
  .text
)edg_test_asm");
#elif defined(__x86_64__)
__asm__(R"edg_test_asm(
  .text
  .globl test_eh_rt_lsda_window_probe
  .p2align 4
test_eh_rt_lsda_window_probe:
  .cfi_startproc
  .cfi_personality 0x9b, DW.ref.__gcc_be_eh_personality_v0
  .cfi_lsda 0x1b, .Ltest_probe_lsda
  pushq %rbp
  .cfi_def_cfa_offset 16
  .cfi_offset %rbp, -16
  movq %rsp, %rbp
  .cfi_def_cfa_register %rbp
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %rax
  .cfi_offset %r12, -24
  .cfi_offset %r13, -32
  .cfi_offset %r14, -40
  movq %rdi, %r12
  movq %rsi, %r13
  movq %rdx, %r14
  xorl %edi, %edi
  callq *%r12
.Ltest_probe_start:
  xorl %edi, %edi
  callq *%r13
.Ltest_probe_end:
  xorl %edi, %edi
  callq *%r14
.Ltest_probe_lp:
  xorl %eax, %eax
  addq $8, %rsp
  popq %r14
  popq %r13
  popq %r12
  popq %rbp
  retq
  .cfi_endproc
  .section .gcc_except_table,"a",@progbits
  .p2align 2
.Ltest_probe_lsda:
  .long .Ltest_probe_start - test_eh_rt_lsda_window_probe
  .long .Ltest_probe_end - test_eh_rt_lsda_window_probe
  .long .Ltest_probe_lp - test_eh_rt_lsda_window_probe
  .text
)edg_test_asm");
#endif

namespace {

/** @brief Synthetic Itanium `__class_type_info` descriptor layout (`16` bytes). */
struct FakeClassTi {
    /** @brief Vtable entry (`&_ZTVN10__cxxabiv117__class_type_infoE[2]`). */
    const void *vptr;
    /** @brief Mangled typeinfo string (`_ZTS...`). */
    const char *name;
};

/** @brief Synthetic Itanium `__si_class_type_info` descriptor layout (`24` bytes). */
struct FakeSiClassTi {
    /** @brief Vtable entry (`&_ZTVN10__cxxabiv120__si_class_type_infoE[2]`). */
    const void *vptr;
    /** @brief Mangled typeinfo string (`_ZTS...`). */
    const char *name;
    /** @brief Direct public base `_ZTI` pointer. */
    const void *base_type;
};

/** @brief Single base entry in `__vmi_class_type_info`. */
struct FakeBaseInfo {
    /** @brief Base `_ZTI` pointer. */
    const void *base_type;
    /** @brief Packed `(offset << 8) | flags`. */
    long offset_flags;
};

/** @brief Synthetic `__vmi_class_type_info` with 20 bases (`base_count > 16`). */
struct FakeVmi20Ti {
    /** @brief Vtable entry (`&_ZTVN10__cxxabiv121__vmi_class_type_infoE[2]`). */
    const void *vptr;
    /** @brief Mangled typeinfo string. */
    const char *name;
    /** @brief Inheritance flags (`0`, `1`, `2`, or `3`). */
    unsigned int flags;
    /** @brief Number of base entries (`20`). */
    unsigned int base_count;
    /** @brief Trailing array of 20 base descriptors. */
    FakeBaseInfo bases[20];
};

/** @brief Synthetic `__pbase_type_info` / `__pointer_type_info` descriptor (`32` bytes). */
struct FakePtrTi {
    /** @brief Vtable entry (`&_ZTVN10__cxxabiv119__pointer_type_infoE[2]`). */
    const void *vptr;
    /** @brief Mangled typeinfo string. */
    const char *name;
    /** @brief CV qualifier mask (`1`=const, `2`=volatile). */
    unsigned int flags;
    /** @brief Alignment padding. */
    unsigned int pad;
    /** @brief Pointee `_ZTI` descriptor. */
    const void *pointee;
};

/** @brief Counter incremented by `terminate_longjmp_handler` on unhandled rethrow. */
static int g_terminate_invocations = 0;
/** @brief Jump buffer restoring control to `test_runtime_lifecycle_and_personality` after `std::terminate()`. */
static std::jmp_buf g_terminate_jmp;

/**
 * @brief `std::terminate_handler` callback recording unhandled `_Unwind_RaiseException` termination and `longjmp`ing back.
 */
[[noreturn]] void terminate_longjmp_handler(void) noexcept {
    ++g_terminate_invocations;
    std::longjmp(g_terminate_jmp, 1);
}

/**
 * @brief No-throw callback used to verify normal `__edg_gcc_eh_invoke_region` completion.
 * @param[in,out] ctx Pointer to an `int` incremented by `42`.
 */
void normal_region_cb(void *ctx) noexcept {
    int *p = static_cast<int *>(ctx);
    if (p) *p += 42;
}

/**
 * @brief Throwing callback exercising `_Unwind_RaiseException` interception by `__edg_gcc_eh_invoke_region`.
 * @param[in] ctx Pointer to the `FakeClassTi` descriptor to throw via `__cxa_throw`.
 */
void throwing_region_cb(void *ctx) noexcept {
    void *mem = __cxxabiv1::__cxa_allocate_exception(sizeof(long));
    *static_cast<long *>(mem) = 123456L;
    __cxxabiv1::__cxa_throw(
        mem,
        reinterpret_cast<std::type_info *>(const_cast<void *>(ctx)),
        nullptr);
}

/**
 * @brief `_Unwind_Backtrace` callback exercising `__gcc_be_eh_personality_v0` across real stack frames.
 * @details Tests frames without LSDA (`!lsda`), frames before `start_pc`, inside `[start_pc, end_pc]`
 * with `actions == 0`, and after `end_pc`.
 * @param[in] ctx Real `_Unwind_Context` provided by `_Unwind_Backtrace`.
 * @param[in] arg Unused opaque pointer.
 * @return `_URC_NO_REASON` to continue walking frames.
 */
_Unwind_Reason_Code backtrace_personality_cb(struct _Unwind_Context *ctx, void *arg) noexcept {
    struct _Unwind_Exception dummy_exc = {};
    int rc = __gcc_be_eh_personality_v0(1, 0, arg ? 1ULL : 0ULL, &dummy_exc, ctx);
    return (rc >= 0) ? _URC_NO_REASON : _URC_NO_REASON;
}

/**
 * @brief Invokes `_Unwind_Backtrace(backtrace_personality_cb, nullptr)` to probe personality branches on live frames.
 * @param[in] unused Unused callback context pointer.
 */
void run_backtrace_probe(void *unused) noexcept {
    _Unwind_Backtrace(&backtrace_personality_cb, unused);
}

/**
 * @brief Thread start routine (intentionally non-`noexcept` so no `__gxx_personality_v0` terminate frame intercepts Phase 1)
 * exercising `_Unwind_RaiseException` returning `_URC_END_OF_STACK` into `_ZSt9terminatev()`.
 * @param[in] arg Pointer to `FakeClassTi` descriptor.
 * @return `nullptr`.
 */
void *unhandled_rethrow_thread_entry(void *arg) noexcept {
    int unw = 0;
    if (__edg_gcc_eh_invoke_region(throwing_region_cb, arg, &unw) != GCC_GEN_BE_SUCCESS) {
        return nullptr;
    }
    if (setjmp(g_terminate_jmp) == 0) {
        gcc_gen_be_error_t unreach = __edg_gcc_eh_resume_or_rethrow();
        if (unreach == GCC_GEN_BE_SUCCESS) return nullptr;
    }
    return nullptr;
}

/**
 * @brief Callback invoking `__edg_gcc_eh_resume_or_rethrow()` inside a guarded outer region.
 * @param[in] unused Unused callback context pointer.
 */
void resume_inside_region_cb(void *unused) noexcept {
    if (!unused) {
        gcc_gen_be_error_t err = __edg_gcc_eh_resume_or_rethrow();
        if (err == GCC_GEN_BE_SUCCESS) return;
    }
}

/**
 * @brief Exercises TLS chain, frame registration, unwind state query, nested catch replacement, and rethrow/terminate.
 * @param[in] leaf_ti Primary `FakeClassTi` descriptor.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t test_runtime_lifecycle_and_personality(FakeClassTi *leaf_ti) noexcept {
    /* TLS chain & frame registration */
    ASSERT_EQ(__edg_gcc_eh_get_tls_chain(nullptr), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    void **chain_slot = nullptr;
    GCC_GEN_BE_CHECK(__edg_gcc_eh_get_tls_chain(&chain_slot));
    ASSERT_TRUE(chain_slot != nullptr);

    ASSERT_EQ(__gcc_be_eh_register_frame(nullptr), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(__gcc_be_eh_unregister_frame(nullptr), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    int fake_fde_a = 1;
    int fake_fde_b = 2;
    GCC_GEN_BE_CHECK(__gcc_be_eh_register_frame(&fake_fde_a));
    ASSERT_TRUE(*chain_slot == &fake_fde_a);
    /* Unregistering non-matching FDE preserves current head */
    GCC_GEN_BE_CHECK(__gcc_be_eh_unregister_frame(&fake_fde_b));
    ASSERT_TRUE(*chain_slot == &fake_fde_a);
    /* Unregistering matching FDE clears head */
    GCC_GEN_BE_CHECK(__gcc_be_eh_unregister_frame(&fake_fde_a));
    ASSERT_TRUE(*chain_slot == nullptr);

    /* Direct personality validation branches */
    struct _Unwind_Exception dummy_exc = {};
    ASSERT_EQ(__gcc_be_eh_personality_v0(2, 0, 0, &dummy_exc, &dummy_exc), (int)_URC_FATAL_PHASE1_ERROR);
    ASSERT_EQ(__gcc_be_eh_personality_v0(1, 0, 0, nullptr, &dummy_exc), (int)_URC_FATAL_PHASE1_ERROR);
    ASSERT_EQ(__gcc_be_eh_personality_v0(1, 0, 0, &dummy_exc, nullptr), (int)_URC_FATAL_PHASE1_ERROR);

    /* Probe !lsda, ip <= start_pc, actions == 0 inside [start_pc, end_pc], and ip > end_pc via _Unwind_Backtrace */
    GCC_GEN_BE_CHECK(test_eh_rt_lsda_window_probe(run_backtrace_probe, run_backtrace_probe, run_backtrace_probe));

    /* OOM on invoke_region */
    int unw = 0;
    int acc = 0;
    g_gcc_be_oom_countdown = 0;
    ASSERT_EQ(__edg_gcc_eh_invoke_region(normal_region_cb, &acc, &unw), GCC_GEN_BE_ERROR_OOM);
    g_gcc_be_oom_countdown = -1;

    /* Empty resume_or_rethrow returns EH_RUNTIME_BRIDGE_FAILED */
    ASSERT_EQ(__edg_gcc_eh_resume_or_rethrow(), GCC_GEN_BE_ERROR_EH_RUNTIME_BRIDGE_FAILED);

    /* Empty depth finish_catch & end_enclosing_catch (both empty-unwind and consumed-catch states) */
    GCC_GEN_BE_CHECK(__edg_gcc_eh_end_enclosing_catch());
    GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(throwing_region_cb, leaf_ti, &unw));
    GCC_GEN_BE_CHECK(__edg_gcc_eh_finish_catch());

    void *tmp_cobj = nullptr;
    GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(throwing_region_cb, leaf_ti, &unw));
    GCC_GEN_BE_CHECK(__edg_gcc_eh_consume_catch(&tmp_cobj));
    GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(throwing_region_cb, leaf_ti, &unw));
    GCC_GEN_BE_CHECK(__edg_gcc_eh_consume_catch(&tmp_cobj));
    GCC_GEN_BE_CHECK(__edg_gcc_eh_finish_catch());
    /* Now depth == 1 while tls_unwind_exc == nullptr -> exercises !tls_unwind_exc branch! */
    GCC_GEN_BE_CHECK(__edg_gcc_eh_end_enclosing_catch());

    GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(throwing_region_cb, leaf_ti, &unw));
    GCC_GEN_BE_CHECK(__edg_gcc_eh_consume_catch(&tmp_cobj));
    GCC_GEN_BE_CHECK(__edg_gcc_eh_end_enclosing_catch());

    /* Same exception rethrown (`throw;` -> handlerCount == 2, tls_is_unwinding == 1) before end_enclosing_catch */
    GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(throwing_region_cb, leaf_ti, &unw));
    GCC_GEN_BE_CHECK(__edg_gcc_eh_consume_catch(&tmp_cobj));
    GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region([](void *) noexcept { __cxxabiv1::__cxa_rethrow(); }, nullptr, &unw));
    ASSERT_EQ(unw, 1);
    GCC_GEN_BE_CHECK(__edg_gcc_eh_end_enclosing_catch());
    GCC_GEN_BE_CHECK(__edg_gcc_eh_finish_catch());

    /* Overflow tls_entered_catch_depth >= 64 branch and drain all 65 real exceptions */
    void *cobj = nullptr;
    for (int i = 0; i < 65; ++i) {
        GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(throwing_region_cb, leaf_ti, &unw));
        ASSERT_EQ(unw, 1);
        ASSERT_EQ(__gcc_be_eh_check_unwind_state(), 1);
        GCC_GEN_BE_CHECK(__edg_gcc_eh_consume_catch(&cobj));
    }
    ASSERT_EQ(__gcc_be_eh_check_unwind_state(), 0);
    for (int i = 0; i < 65; ++i) {
        GCC_GEN_BE_CHECK(__edg_gcc_eh_finish_catch());
    }

    /* Nested catch throwing a BRAND NEW exception E2 inside catch(E1): exercises end_enclosing_catch replacement + OOM! */
    GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(throwing_region_cb, leaf_ti, &unw));
    GCC_GEN_BE_CHECK(__edg_gcc_eh_consume_catch(&cobj));
    FakeClassTi second_ti = { &_ZTVN10__cxxabiv117__class_type_infoE[2], "9SecondObj" };
    GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(throwing_region_cb, &second_ti, &unw));
    ASSERT_EQ(unw, 1);
    /* First test OOM on end_enclosing_catch (push E1 back first if popped -> re-consume + re-set) */
    g_gcc_be_oom_countdown = 0;
    ASSERT_EQ(__edg_gcc_eh_end_enclosing_catch(), GCC_GEN_BE_ERROR_OOM);
    g_gcc_be_oom_countdown = -1;
    /* Now succeed on end_enclosing_catch (depth==0 so prev_exc==nullptr != tls_unwind_exc -> destroys E1, keeps E2!) */
    GCC_GEN_BE_CHECK(__edg_gcc_eh_end_enclosing_catch());
    GCC_GEN_BE_CHECK(__edg_gcc_eh_consume_catch(&cobj));
    GCC_GEN_BE_CHECK(__edg_gcc_eh_finish_catch());

    /* Resume/rethrow caught by an outer invoke_region, plus OOM on resume_or_rethrow */
    GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(throwing_region_cb, leaf_ti, &unw));
    g_gcc_be_oom_countdown = 0;
    ASSERT_EQ(__edg_gcc_eh_resume_or_rethrow(), GCC_GEN_BE_ERROR_OOM);
    g_gcc_be_oom_countdown = -1;
    GCC_GEN_BE_CHECK(__edg_gcc_eh_consume_catch(&cobj));
    GCC_GEN_BE_CHECK(__edg_gcc_eh_finish_catch());

    GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(throwing_region_cb, leaf_ti, &unw));
    int outer_unw = 0;
    GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(resume_inside_region_cb, nullptr, &outer_unw));
    ASSERT_EQ(outer_unw, 1);
    GCC_GEN_BE_CHECK(__edg_gcc_eh_consume_catch(&cobj));
    GCC_GEN_BE_CHECK(__edg_gcc_eh_finish_catch());

    /* Unhandled top-level resume_or_rethrow on a clean pthread stack -> _URC_END_OF_STACK -> _ZSt9terminatev()! */
    std::terminate_handler prev_term = std::set_terminate(&terminate_longjmp_handler);
    pthread_t th;
    ASSERT_EQ(pthread_create(&th, nullptr, &unhandled_rethrow_thread_entry, leaf_ti), 0);
    ASSERT_EQ(pthread_join(th, nullptr), 0);
    std::set_terminate(prev_term);
    ASSERT_EQ(g_terminate_invocations, 1);

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Runs all `_ZTI` matching, VMI flags `0..3`, `base_count == 20`, pointer CV, void*, and unwind tests.
 * @param[out] out_ok Pointer receiving `1` on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t run_eh_rt_unit_suite(int *out_ok) noexcept {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    /* 1. Invalid argument checks */
    int unw = 0;
    ASSERT_EQ(__edg_gcc_eh_invoke_region(nullptr, nullptr, &unw), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(__edg_gcc_eh_invoke_region(normal_region_cb, nullptr, nullptr), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    void *exc_out = nullptr;
    void *ti_out = nullptr;
    ASSERT_EQ(__edg_gcc_eh_get_unwind_info(nullptr, &ti_out), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(__edg_gcc_eh_get_unwind_info(&exc_out, nullptr), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(__edg_gcc_eh_consume_catch(nullptr), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);

    /* 2. Normal region execution */
    int acc = 10;
    GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(normal_region_cb, &acc, &unw));
    ASSERT_EQ(unw, 0);
    ASSERT_EQ(acc, 52);

    /* 3. Throwing region interception + lifecycle & personality suite */
    FakeClassTi leaf_ti = { &_ZTVN10__cxxabiv117__class_type_infoE[2], "7LeafObj" };
    GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(throwing_region_cb, &leaf_ti, &unw));
    ASSERT_EQ(unw, 1);
    GCC_GEN_BE_CHECK(__edg_gcc_eh_get_unwind_info(&exc_out, &ti_out));
    ASSERT_TRUE(exc_out != nullptr);
    ASSERT_EQ(*static_cast<long *>(exc_out), 123456L);
    ASSERT_TRUE(ti_out == &leaf_ti);
    void *consumed_obj = nullptr;
    GCC_GEN_BE_CHECK(__edg_gcc_eh_consume_catch(&consumed_obj));
    ASSERT_TRUE(consumed_obj == exc_out);
    GCC_GEN_BE_CHECK(__edg_gcc_eh_finish_catch());
    GCC_GEN_BE_CHECK(__edg_gcc_eh_finish_catch());

    GCC_GEN_BE_CHECK(test_runtime_lifecycle_and_personality(&leaf_ti));

    /* 4. Catch-all (`catch_type == nullptr`), `throw_type == nullptr`, null name, fundamental vptr */
    unsigned char dummy_obj[256] = {};
    ASSERT_TRUE(__gcc_be_eh_match_catch(nullptr, &leaf_ti, dummy_obj) == dummy_obj);
    ASSERT_TRUE(__gcc_be_eh_match_catch(nullptr, &leaf_ti, nullptr) == reinterpret_cast<void *>(static_cast<uintptr_t>(1)));
    ASSERT_TRUE(__gcc_be_eh_match_catch(&leaf_ti, nullptr, dummy_obj) == nullptr);
    ASSERT_TRUE(__gcc_be_eh_match_catch(&leaf_ti, &leaf_ti, nullptr) == reinterpret_cast<void *>(static_cast<uintptr_t>(1)));

    FakeClassTi leaf_dup_name = { &_ZTVN10__cxxabiv117__class_type_infoE[0], "7LeafObj" };
    FakeSiClassTi si_v0 = { &_ZTVN10__cxxabiv120__si_class_type_infoE[0], "9SiVtable0", &leaf_dup_name };
    FakePtrTi ptr_v0 = { &_ZTVN10__cxxabiv119__pointer_type_infoE[0], "P7LeafObj", 0U, 0U, &leaf_dup_name };
    FakeClassTi null_name_a = { nullptr, nullptr };
    FakeClassTi null_name_b = { nullptr, nullptr };
    ASSERT_TRUE(__gcc_be_eh_match_catch(&leaf_ti, &leaf_dup_name, dummy_obj) == dummy_obj);
    ASSERT_TRUE(__gcc_be_eh_match_catch(&leaf_dup_name, &si_v0, dummy_obj) == dummy_obj);
    void *v0_slot = dummy_obj;
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_v0, &null_name_a, &v0_slot) == nullptr);
    ASSERT_TRUE(__gcc_be_eh_match_catch(&null_name_a, &leaf_dup_name, dummy_obj) == nullptr);
    ASSERT_TRUE(__gcc_be_eh_match_catch(&null_name_a, &leaf_ti, dummy_obj) == nullptr);
    ASSERT_TRUE(__gcc_be_eh_match_catch(&leaf_ti, &null_name_b, dummy_obj) == nullptr);

    /* 5. __vmi_class_type_info with flags == 0, 1, 2, 3, base_count == 20, diamond & ambiguous repeat */
    FakeClassTi bases_ti[20];
    char names[20][16];
    FakeVmi20Ti vmi_ti = {};
    vmi_ti.vptr = &_ZTVN10__cxxabiv121__vmi_class_type_infoE[2];
    vmi_ti.name = "9WideVmi20";
    vmi_ti.base_count = 20U;
    for (int i = 0; i < 20; ++i) {
        names[i][0] = '2';
        names[i][1] = 'B';
        names[i][2] = static_cast<char>('A' + i);
        names[i][3] = '\0';
        bases_ti[i].vptr = &_ZTVN10__cxxabiv117__class_type_infoE[2];
        bases_ti[i].name = names[i];
        vmi_ti.bases[i].base_type = &bases_ti[i];
        vmi_ti.bases[i].offset_flags = (static_cast<long>(i * 8) << 8) | 0x2L;
    }

    for (unsigned int flg = 0U; flg <= 3U; ++flg) {
        vmi_ti.flags = flg;
        void *matched = __gcc_be_eh_match_catch(&bases_ti[18], &vmi_ti, dummy_obj);
        ASSERT_TRUE(matched == static_cast<void *>(dummy_obj + 144));
    }
    vmi_ti.vptr = &_ZTVN10__cxxabiv121__vmi_class_type_infoE[0];
    ASSERT_TRUE(__gcc_be_eh_match_catch(&bases_ti[18], &vmi_ti, dummy_obj) == static_cast<void *>(dummy_obj + 144));
    vmi_ti.vptr = &_ZTVN10__cxxabiv121__vmi_class_type_infoE[2];
    /* curr_obj == nullptr through VMI */
    ASSERT_TRUE(__gcc_be_eh_match_catch(&bases_ti[2], &vmi_ti, nullptr) != nullptr);

    /* Null base_type and non-public base */
    vmi_ti.bases[19].base_type = nullptr;
    FakeClassTi unreached_ti = { &_ZTVN10__cxxabiv117__class_type_infoE[2], "9Unreached" };
    ASSERT_TRUE(__gcc_be_eh_match_catch(&unreached_ti, &vmi_ti, dummy_obj) == nullptr);
    vmi_ti.bases[19].base_type = &bases_ti[19];
    vmi_ti.bases[19].offset_flags = (19L * 8L) << 8;
    ASSERT_TRUE(__gcc_be_eh_match_catch(&bases_ti[19], &vmi_ti, dummy_obj) == nullptr);

    /* Diamond same-subobject match (state == 1 && *io_found_obj == curr_obj) AND ambiguous repeat (*io_found_obj != curr_obj) */
    vmi_ti.flags = 3U;
    vmi_ti.bases[0].base_type = &bases_ti[0];
    vmi_ti.bases[0].offset_flags = (8L << 8) | 0x3L; /* Virtual with non-negative offset +8 */
    vmi_ti.bases[1].base_type = &bases_ti[0];
    vmi_ti.bases[1].offset_flags = (8L << 8) | 0x3L; /* Same subobject +8 -> unique diamond match! */
    ASSERT_TRUE(__gcc_be_eh_match_catch(&bases_ti[0], &vmi_ti, dummy_obj) == static_cast<void *>(dummy_obj + 8));

    vmi_ti.bases[1].offset_flags = (24L << 8) | 0x2L; /* Distinct subobject +24 -> ambiguous -> nullptr! */
    ASSERT_TRUE(__gcc_be_eh_match_catch(&bases_ti[0], &vmi_ti, dummy_obj) == nullptr);

    /* Virtual base adjustment via vtable negative offset (`flags & 0x1` and `off < 0`) */
    intptr_t fake_vtbl_storage[4] = { 64, 0, 0, 0 };
    const void *vptr_at_obj = &fake_vtbl_storage[2];
    *reinterpret_cast<const void **>(dummy_obj) = vptr_at_obj;
    FakeClassTi vbase_ti = { &_ZTVN10__cxxabiv117__class_type_infoE[2], "5VBase" };
    vmi_ti.bases[0].base_type = &vbase_ti;
    vmi_ti.bases[0].offset_flags = static_cast<long>((static_cast<unsigned long>(-16L) << 8) | 0x3UL);
    ASSERT_TRUE(__gcc_be_eh_match_catch(&vbase_ti, &vmi_ti, dummy_obj) == static_cast<void *>(dummy_obj + 64));

    /* 6. Single inheritance (with non-null and null base_type) & pointer CV / void* / function-ptr conversions */
    FakeSiClassTi si_ti = { &_ZTVN10__cxxabiv120__si_class_type_infoE[2], "8SiChild", &leaf_ti };
    FakeSiClassTi si_null_base = { &_ZTVN10__cxxabiv120__si_class_type_infoE[2], "9SiOrphan", nullptr };
    ASSERT_TRUE(__gcc_be_eh_match_catch(&leaf_ti, &si_ti, dummy_obj) == static_cast<void *>(dummy_obj));
    ASSERT_TRUE(__gcc_be_eh_match_catch(&leaf_ti, &si_null_base, dummy_obj) == nullptr);

    FakeClassTi void_ti = { nullptr, "v" };
    FakeClassTi fn_ti = { nullptr, "FvvE" };
    FakePtrTi ptr_void = { &_ZTVN10__cxxabiv119__pointer_type_infoE[2], "Pv", 0U, 0U, &void_ti };
    FakePtrTi ptr_null_ptee = { &_ZTVN10__cxxabiv119__pointer_type_infoE[2], "Pv0", 0U, 0U, nullptr };
    FakePtrTi ptr_fn = { &_ZTVN10__cxxabiv119__pointer_type_infoE[2], "PFvvE", 0U, 0U, &fn_ti };
    FakePtrTi ptr_null_name_ptee = { &_ZTVN10__cxxabiv119__pointer_type_infoE[2], "Pu", 0U, 0U, &null_name_a };
    FakePtrTi ptr_nonconst = { &_ZTVN10__cxxabiv119__pointer_type_infoE[2], "P7LeafObj", 0U, 0U, &leaf_ti };
    FakePtrTi ptr_const = { &_ZTVN10__cxxabiv119__pointer_type_infoE[2], "PK7LeafObj", 1U, 0U, &leaf_ti };
    FakePtrTi ptr_const_dup = { &_ZTVN10__cxxabiv119__pointer_type_infoE[2], "PK7LeafObj2", 1U, 0U, &leaf_dup_name };
    FakePtrTi ptr_si = { &_ZTVN10__cxxabiv119__pointer_type_infoE[2], "P8SiChild", 0U, 0U, &si_ti };
    FakePtrTi ptr_si_orphan = { &_ZTVN10__cxxabiv119__pointer_type_infoE[2], "P9SiOrphan", 0U, 0U, &si_null_base };

    void *target_val = dummy_obj + 16;
    void *ptr_slot = target_val;
    /* Non-pointer thrown vs pointer catch */
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_const, &leaf_ti, &ptr_slot) == nullptr);
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_const, &null_name_a, &ptr_slot) == nullptr);
    /* Null pointee cases */
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_null_ptee, &ptr_nonconst, &ptr_slot) == nullptr);
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_const, &ptr_null_ptee, &ptr_slot) == nullptr);
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_null_name_ptee, &ptr_nonconst, &ptr_slot) == nullptr);
    /* Object pointer -> void* succeeds; function pointer -> void* fails! */
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_void, &ptr_nonconst, &ptr_slot) != nullptr);
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_void, &ptr_null_name_ptee, &ptr_slot) != nullptr);
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_void, &ptr_fn, &ptr_slot) == nullptr);
    /* T* -> const T* (same pointee address and duplicate pointee name) */
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_const, &ptr_nonconst, &ptr_slot) != nullptr);
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_const_dup, &ptr_nonconst, &ptr_slot) != nullptr);
    /* const T* -> T* fails */
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_nonconst, &ptr_const, &ptr_slot) == nullptr);
    /* Derived* -> const Base* with non-null slot, null deref_ptr, null obj_ptr, and unrelated class* */
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_const, &ptr_si, &ptr_slot) != nullptr);
    void *null_ptr_slot = nullptr;
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_const, &ptr_si, &null_ptr_slot) != nullptr);
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_const, &ptr_si, nullptr) != nullptr);
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_const, &ptr_si_orphan, &ptr_slot) == nullptr);
    /* std::nullptr_t (`Dn`) -> pointer catch */
    FakeClassTi nullptr_ti = { &_ZTVN10__cxxabiv117__class_type_infoE[2], "Dn" };
    ASSERT_TRUE(__gcc_be_eh_match_catch(&ptr_const, &nullptr_ti, &null_ptr_slot) != nullptr);

    /* 7. Deterministic OOM sweep across match_abi_type_info_full */
    for (int b = 0; b < 25; ++b) {
        g_gcc_be_oom_countdown = b;
        void *res = __gcc_be_eh_match_catch(&ptr_const, &ptr_si, &ptr_slot);
        g_gcc_be_oom_countdown = -1;
        if (res != nullptr) break;
    }
    for (int b = 0; b < 25; ++b) {
        g_gcc_be_oom_countdown = b;
        void *res = __gcc_be_eh_match_catch(&vbase_ti, &vmi_ti, dummy_obj);
        g_gcc_be_oom_countdown = -1;
        if (res != nullptr) break;
    }
    for (int b = 0; b < 15; ++b) {
        g_gcc_be_oom_countdown = b;
        void *res = __gcc_be_eh_match_catch(&ptr_void, &ptr_nonconst, &ptr_slot);
        g_gcc_be_oom_countdown = -1;
        if (res != nullptr) break;
    }
    for (int b = 0; b < 15; ++b) {
        g_gcc_be_oom_countdown = b;
        void *res = __gcc_be_eh_match_catch(&leaf_ti, &si_ti, dummy_obj);
        g_gcc_be_oom_countdown = -1;
        if (res != nullptr) break;
    }

    /* 8. Nested unwinding during active unwind (same exc vs distinct exc + dependent 0x01 GNUC/CLNG exception_class + finish_catch restore) */
    {
        int unw = 0;
        void *cobj = nullptr;
        /* Outer unwind #1 -> sets tls_is_unwinding = 1 */
        GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(throwing_region_cb, &leaf_ti, &unw));
        ASSERT_TRUE(unw == 1);

        /* Re-throw SAME active exception while tls_is_unwinding == 1 (covers 683:9 True, 683:29 False!) */
        GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region([](void *) noexcept { __cxxabiv1::__cxa_rethrow(); }, nullptr, &unw));
        ASSERT_TRUE(unw == 1);
        __cxxabiv1::__cxa_end_catch();

        /* Throw DISTINCT exception while tls_is_unwinding == 1 (covers 683:9 True, 683:29 True -> pushes saved unwind!) */
        FakeClassTi second_ti = { &_ZTVN10__cxxabiv117__class_type_infoE[2], "9SecondObj" };
        GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(throwing_region_cb, &second_ti, &unw));
        ASSERT_TRUE(unw == 1);

        /* Finish E2 (restores E1 via 736:9 True!) and then finish E1 (736:9 False!) */
        GCC_GEN_BE_CHECK(__edg_gcc_eh_consume_catch(&cobj));
        GCC_GEN_BE_CHECK(__edg_gcc_eh_finish_catch());
        GCC_GEN_BE_CHECK(__edg_gcc_eh_consume_catch(&cobj));
        GCC_GEN_BE_CHECK(__edg_gcc_eh_finish_catch());

        /* Dependent exception classes (`0x474E5543...01` GNUC & `0x434C4E47...01` CLNG) backed by real __cxa_allocate_exception headers */
        auto raise_dep_class = [&](uint64_t exc_cls) noexcept -> gcc_gen_be_error_t {
            void *prim_mem = __cxxabiv1::__cxa_allocate_exception(16);
            alignas(64) void *dep_storage[32] = {};
            _Unwind_Exception *uhdr = reinterpret_cast<_Unwind_Exception *>(&dep_storage[16]);
#if defined(__APPLE__)
            if (exc_cls == 0x434C4E47432B2B01ULL) {
                __cxxabiv1::__cxa_increment_exception_refcount(prim_mem);
                __cxxabiv1::__cxa_increment_exception_refcount(prim_mem);
                void *dep_mem = __cxxabiv1::__cxa_allocate_exception(64);
                uhdr = reinterpret_cast<_Unwind_Exception *>(dep_mem) - 1;
            }
#endif
            void **uhdr_words = reinterpret_cast<void **>(uhdr);
            uhdr_words[-10] = prim_mem;
            uhdr_words[-11] = prim_mem;
            uhdr->exception_class = exc_cls;
            uhdr->exception_cleanup = nullptr;
            int u = 0;
            GCC_GEN_BE_CHECK(__edg_gcc_eh_invoke_region(
                [](void *p) noexcept { _Unwind_RaiseException(static_cast<_Unwind_Exception *>(p)); },
                uhdr,
                &u));
            void *co = nullptr;
            GCC_GEN_BE_CHECK(__edg_gcc_eh_consume_catch(&co));
            __cxxabiv1::__cxa_end_catch();
#if defined(__APPLE__)
            if (exc_cls == 0x434C4E47432B2B01ULL) {
                __cxxabiv1::__cxa_decrement_exception_refcount(prim_mem);
            } else {
                __cxxabiv1::__cxa_free_exception(prim_mem);
            }
#else
            __cxxabiv1::__cxa_free_exception(prim_mem);
#endif
            return (u == 1 && co == prim_mem) ? GCC_GEN_BE_SUCCESS : GCC_GEN_BE_ERROR_INTERNAL;
        };
        GCC_GEN_BE_CHECK(raise_dep_class(0x474E5543432B2B01ULL));
        GCC_GEN_BE_CHECK(raise_dep_class(0x434C4E47432B2B01ULL));
    }

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

} // namespace

/**
 * @brief Entry point for `test_gcc_be_eh_rt_unit.cpp`.
 * @details Runs `run_eh_rt_unit_suite` and returns `0` on success.
 * @return `0` on success, `1` on failure.
 */
int main() noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_eh_rt_unit_suite(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok == 1) ? 0 : 1;
}
