# Internals of the EDG Multi-Backend Compiler Architecture: Native LLVM IR and GCC (`libgccjit`) Code Generators in `cpfe`

> **Comparative Baseline**: Branch `multibackend` (`2f5df72de8`, tag `v7.0.1`) vs. upstream branch `main` (`ad4cfc4018`)  
> **Repository**: Edison Design Group C/C++ Front End (`edgcpp-compiler-multibackend`)  
> **Scope**: Exhaustive architectural, algorithmic, ABI, runtime, build-system, and empirical systems specification intended as the technical foundation for an academic compiler engineering paper.

---

## Abstract

For nearly four decades, the Edison Design Group C/C++ Front End (`cpfe`) operated strictly as a front-end semantic analyzer and source-to-source transpiler, delegating machine code generation to downstream host compilers via either lowered K&R/ISO C emission (`c_gen_be`, 12,333 LOC) or unlowered C++ reconstruction (`cp_gen_be`, 27,093 LOC). While source-to-source lowering maximizes portability, it imposes fundamental semantic and performance ceilings: zero-cost DWARF/SEH exception handling degrades into portable `setjmp`/`longjmp` runtime tables, C++20 coroutine frames cannot exploit SSA mid-end frame-splitting passes (`CoroSplit`), fine-grained LLVM/GIMPLE alias metadata (TBAA), sanitizer instrumentation, ThinLTO summaries, GPU offloading (`nvptx64`/`amdgcn`), and DWARF v5 / CodeView debug info cannot be faithfully expressed through generated C text, and compilation latency suffers from double lexing and parsing.

The `multibackend` architecture transforms `cpfe` from a source-to-source front end into a **single-binary, runtime-selectable multi-backend native C/C++23 compiler** hosting four distinct code generation pipelines:
1. **LLVM IR Backend (`src/llvm_gen_be*`, 63 files, 32,716 LOC)**: A direct C++17 SSA IR generator targeting LLVM 16 through 23+, implementing 12 hardware psABIs, dual Itanium/Microsoft C++ object models, zero-cost DWARF and Windows funclet SEH unwinding, switch-resumed `llvm.coro.*` coroutines, OpenMP 4.5/5.x CPU microtasks and GPU target offloading, GNU + Intel/MSVC inline assembly translation, DWARF v4/v5 + Split DWARF + CodeView debug metadata, and New Pass Manager (`PassBuilder`) PGO/LTO/Sanitizer pipelines.
2. **GCC (`libgccjit`) Backend (`src/gcc_gen_be_*`, 32 files, 26,713 LOC + `lib_src/gcc_be_eh_rt.c`, 249 LOC)**: A C-compatible typed-GIMPLE code generator targeting GCC 10 through 15+ via zero-link-dependency dynamic symbol loading (`dlopen`/`LoadLibraryA`) and runtime API capability probing, featuring 9 hardware psABI classifiers, explicit AST-to-GIMPLE C++20 coroutine state-machine and HALO synthesis, a dual-mode exception handling engine (adaptive GIMPLE EH + custom binary DWARF LSDA / Win64 SEH / ARM EHABI table encoders paired with a hybrid SJLJ + Itanium RTTI inheritance-matching runtime bridge), IEEE 754-2008 Decimal Floating-Point (BID/DPD), and ISO/IEC TR 18037 Fixed-Point arithmetic.
3. **Legacy C-Generating Backend (`src/c_gen_be.*`, 12,333 LOC)** and **C++-Generating Backend (`src/cp_gen_be.*`, 27,093 LOC)**: Preserved with 100% backward compatibility and selectable alongside native backends via orthogonal CMake build modes and runtime `--backend={llvm,gcc,c,cpp}` CLI dispatch.

Crucially, this 106,553-line expansion was achieved with **surgical non-invasiveness**: across the entire pre-existing front end under `src/`, only **28 legacy files** were touched (totalling 153 deleted/modified lines repo-wide). This monograph documents every subsystem, data structure, lowering invariant, ABI state machine, runtime protocol, and empirical bug discovered during real-world validation.

---

## Table of Contents

1. [Quantitative Repository Delta & Complete File Inventory](#1-quantitative-repository-delta--complete-file-inventory)
   - 1.1 [High-Level Branch Comparison (`main` vs. `multibackend`)](#11-high-level-branch-comparison-main-vs-multibackend)
   - 1.2 [Complete File Inventory: LLVM IR Backend (`src/llvm_gen_be*` — 63 Files, 32,716 LOC)](#12-complete-file-inventory-llvm-ir-backend-srcllvm_gen_be--63-files-32716-loc)
   - 1.3 [Complete File Inventory: GCC (`libgccjit`) Backend (`src/gcc_gen_be_*` — 32 Files, 26,713 LOC + Runtime)](#13-complete-file-inventory-gcc-libgccjit-backend-srcgcc_gen_be_--32-files-26713-loc--runtime)
2. [Multi-Backend Integration into the EDG Front End (`cpfe`)](#2-multi-backend-integration-into-the-edg-front-end-cpfe)
   - 2.1 [End-to-End Compilation Pipeline and the Asymmetric IL Lowering Boundary](#21-end-to-end-compilation-pipeline-and-the-asymmetric-il-lowering-boundary)
   - 2.2 [Preprocessor Configuration Lattice (`defines.h`, `host_envir.h`, `targ_def.h`, `lang_feat.h`)](#22-preprocessor-configuration-lattice-definesh-host_envirh-targ_defh-lang_feath)
   - 2.3 [Translation Unit Dispatch and Multi-TU Orchestration (`src/cfe.c`, `src/fe_wrapup.c`)](#23-translation-unit-dispatch-and-multi-tu-orchestration-srccfec-srcfe_wrapupc)
   - 2.4 [Catalogue of All 28 Modified Legacy Front-End Files in `src/`](#24-catalogue-of-all-28-modified-legacy-front-end-files-in-src)
3. [Build System, Macro Configuration (`macro-conf`), and Portability Layer](#3-build-system-macro-configuration-macro-conf-and-portability-layer)
   - 3.1 [The 4-Mode CMake Matrix (`CMakeLists.txt`, `cmake/macro-conf.cmake`)](#31-the-4-mode-cmake-matrix-cmakeliststxt-cmakemacro-confcmake)
   - 3.2 [Cross-Platform `FindLibGCCJIT.cmake` and MSVC Header Shadow Prevention](#32-cross-platform-findlibgccjitcmake-and-msvc-header-shadow-prevention)
   - 3.3 [Portable Linux Binary Compatibility Layer (`src/linux_libc_compat.c`)](#33-portable-linux-binary-compatibility-layer-srclinux_libc_compatc)
4. [Total Error Percolation & `noexcept` Reliability Architecture](#4-total-error-percolation--noexcept-reliability-architecture)
   - 4.1 [LLVM Backend Error Subsystem (`src/llvm_gen_be_error.{h,cpp}`)](#41-llvm-backend-error-subsystem-srcllvm_gen_be_errorhcpp)
   - 4.2 [GCC Backend Error Subsystem (`src/gcc_gen_be_error.{h,c}`)](#42-gcc-backend-error-subsystem-srcgcc_gen_be_errorhc)
5. [Internals of the LLVM IR Backend (`src/llvm_gen_be*`)](#5-internals-of-the-llvm-ir-backend-srcllvm_gen_be)
   - 5.1 [Core State, Lifecycle, and Two-Pass Translation Unit Lowering](#51-core-state-lifecycle-and-two-pass-translation-unit-lowering)
   - 5.2 [Multi-Version LLVM C++ API Compatibility Layer (LLVM 16–23+)](#52-multi-version-llvm-c-api-compatibility-layer-llvm-1623)
   - 5.3 [Type System, Bitfield Storage Synthesis, Address Spaces, and TBAA](#53-type-system-bitfield-storage-synthesis-address-spaces-and-tbaa)
   - 5.4 [Host-Independent Constant Evaluator](#54-host-independent-constant-evaluator-srcllvm_gen_be_consthcpp)
   - 5.5 [Target ABI Subsystem Across 12 Hardware Architectures](#55-target-abi-subsystem-across-12-hardware-architectures)
   - 5.6 [Expression, Statement, and Target SIMD/System Intrinsics Lowering](#56-expression-statement-and-target-simdsystem-intrinsics-lowering)
   - 5.7 [Dual-ABI C++ Object Model: VTables, RTTI, and Pointers-to-Members](#57-dual-abi-c-object-model-vtables-rtti-and-pointers-to-members)
   - 5.8 [Zero-Cost Exception Handling: Itanium DWARF and Windows Funclet SEH](#58-zero-cost-exception-handling-itanium-dwarf-and-windows-funclet-seh)
   - 5.9 [C++20 Coroutines via Switch-Resumed `CoroSplit` Intrinsics](#59-c20-coroutines-via-switch-resumed-corosplit-intrinsics)
   - 5.10 [OpenMP 4.5 / 5.x Outlining and GPU Target Offloading](#510-openmp-45--5x-outlining-and-gpu-target-offloading)
   - 5.11 [Inline Assembly: GNU `callbr` and Intel/MSVC Dialect Translator](#511-inline-assembly-gnu-callbr-and-intelmsvc-dialect-translator)
   - 5.12 [Debug Information: DWARF v4/v5, Split DWARF, and CodeView](#512-debug-information-dwarf-v4v5-split-dwarf-and-codeview)
   - 5.13 [Optimization Pipeline (`PassBuilder`), Sanitizers, PGO, LTO, and Driver](#513-optimization-pipeline-passbuilder-sanitizers-pgo-lto-and-driver)
6. [Internals of the GCC (`libgccjit`) Backend (`src/gcc_gen_be_*`)](#6-internals-of-the-gcc-libgccjit-backend-srcgcc_gen_be_)
   - 6.1 [Architectural Philosophy and the `libgccjit` Constraint Space](#61-architectural-philosophy-and-the-libgccjit-constraint-space)
   - 6.2 [Zero-Dependency Dynamic Loader and Runtime Capability Probing](#62-zero-dependency-dynamic-loader-and-runtime-capability-probing-gcc_gen_be_lib_loaderhc)
   - 6.3 [Context Stacks and the 8-Domain Open-Chaining Hash Cache](#63-context-stacks-and-the-8-domain-open-chaining-hash-cache-gcc_gen_be_context-gcc_gen_be_cache)
   - 6.4 [Topological Struct Cycle Breaking, Bitfields, Decimal FP (BID/DPD), and Fixed-Point](#64-topological-struct-cycle-breaking-bitfields-decimal-fp-biddpd-and-fixed-point-gcc_gen_be_typec)
   - 6.5 [Multi-Target ABI Classification and Register Coercion](#65-multi-target-abi-classification-and-register-coercion-gcc_gen_be_abihc)
   - 6.6 [Computed Gotos, Inline Asm Constraint Splitting, and Explicit C++20 Coroutines](#66-computed-gotos-inline-asm-constraint-splitting-and-explicit-c20-coroutines-gcc_gen_be_stmtc)
   - 6.7 [Dual-Mode Exception Handling, Unwind Table Encoders, and Hybrid Runtime](#67-dual-mode-exception-handling-unwind-table-encoders-and-hybrid-runtime-gcc_gen_be_ehc--lib_srcgcc_be_eh_rtc)
   - 6.8 [Declarations, `__dso_handle`, Expressions, Array Cookies, and Builtins](#68-declarations-__dso_handle-expressions-array-cookies-and-builtins)
   - 6.9 [Itanium C++ ABI Substitution Mangler, VTables, VTTs, Thunks, and RTTI](#69-itanium-c-abi-substitution-mangler-vtables-vtts-thunks-and-rtti-gcc_gen_be_rttic)
   - 6.10 [Debug Locations and Rich DWARF Metadata Synthesis](#610-debug-locations-and-rich-dwarf-metadata-synthesis-gcc_gen_be_locationc-gcc_gen_be_debugc)
   - 6.11 [Main Orchestrator, `eccp.sh` Driver, and `lib_src/` Runtime Integration](#611-main-orchestrator-eccpsh-driver-and-lib_src-runtime-integration)
7. [Head-to-Head Architectural Synthesis: LLVM vs. GCC vs. Legacy Backends](#7-head-to-head-architectural-synthesis-llvm-vs-gcc-vs-legacy-backends)
8. [Verification Methodology, 100% Coverage Gates, and Empirical Corpus Study](#8-verification-methodology-100-coverage-gates-and-empirical-corpus-study)
   - 8.1 [Test Suite Architecture (`tests/` — 88 Backend Test Files, 37,352 LOC)](#81-test-suite-architecture-tests--88-backend-test-files-37352-loc)
   - 8.2 [Automated 100% Documentation and Code Coverage Gates (`dev_tools/`)](#82-automated-100-documentation-and-code-coverage-gates-dev_tools)
   - 8.3 [Empirical Case Study: Real-World Bug Taxonomy Exposed by `sqlite3.c` (255k LOC) and Cross-OS CI](#83-empirical-case-study-real-world-bug-taxonomy-exposed-by-sqlite3c-255k-loc-and-cross-os-ci)
9. [Research Takeaways and Open Compiler Engineering Problems](#9-research-takeaways-and-open-compiler-engineering-problems)
- [Appendix A: Complete Multi-Backend CLI Option Reference](#appendix-a-complete-multi-backend-cli-option-reference-srccmd_linehc)
- [Appendix B: Complete `gcc_gen_be_error_t` Enumerator Reference](#appendix-b-complete-gcc_gen_be_error_t-enumerator-reference-srcgcc_gen_be_errorh)
- [Appendix C: Branch Lineage, Multi-OS Release Matrix, and Reproducibility Guide](#appendix-c-branch-lineage-multi-os-release-matrix-and-reproducibility-guide)

---

## 1. Quantitative Repository Delta & Complete File Inventory

### 1.1 High-Level Branch Comparison (`main` vs. `multibackend`)

Comparing `main` (`ad4cfc4018`) directly against `multibackend` (`2f5df72de8`) via two-tree `git diff main multibackend` yields **309 files changed, 106,553 insertions(+), and 153 deletions(-)** across 35 integration and remediation commits (merging and unifying the standalone `llvm_backend` and `gcc_backend` development lineages).

| Architectural Component | Path Pattern | File Count | Total LOC | Status vs. `main` |
| :--- | :--- | ---: | ---: | :--- |
| **EDG Front-End Semantic Lowering** | `src/lower_*` | 11 | 74,623 | Pre-existing (1 line changed in `lower_il.c`) |
| **Legacy C++-Generating Backend** | `src/cp_gen_be.{h,c}` | 2 | 27,093 | Pre-existing (+13 lines for guard & `availability` attr) |
| **Legacy C-Generating Backend** | `src/c_gen_be.{h,c}` | 2 | 12,333 | Pre-existing (+5 lines for `!BACK_END_IS_LLVM_GEN_BE`) |
| **New LLVM IR Backend** | `src/llvm_gen_be*.{h,cpp}` | **63** | **32,716** | **100% New in `multibackend`** |
| **New GCC (`libgccjit`) Backend** | `src/gcc_gen_be_*.{h,c}` | **32** | **26,713** | **100% New in `multibackend`** |
| **GCC Hybrid SJLJ/Itanium EH Runtime** | `lib_src/gcc_be_eh_rt.c` | **1** | **249** | **100% New in `multibackend`** |
| **Linux glibc 2.38+ / musl Shim** | `src/linux_libc_compat.c` | **1** | **173** | **100% New in `multibackend`** |
| **Modified Legacy Front-End Files** | `src/*.{h,c,cpp}` | **28** | — | Surgical hooks (+1,089 / -89 lines across 28 files) |
| **LLVM Backend Test & Fuzz Suite** | `tests/*llvm*` | **41** | **25,326** | **100% New in `multibackend`** |
| **GCC Backend Unit & Execution Suite** | `tests/gcc_be/*` | **47** | **12,026** | **100% New in `multibackend`** |
| **Build, Presets, CI & Coverage Tools** | `cmake/`, `dev_tools/`, `.github/` | **86** | **~8,900** | Multi-backend CMake, `FindLibGCCJIT`, CI/release workflows |

### 1.2 Complete File Inventory: LLVM IR Backend (`src/llvm_gen_be*` — 63 Files, 32,716 LOC)

| Subsystem | Header File (LOC) | Implementation File (LOC) | Combined LOC | Core Responsibility |
| :--- | :--- | :--- | ---: | :--- |
| **Orchestrator & Lifecycle** | `llvm_gen_be.h` (97)<br>`llvm_gen_be_internal.h` (449) | `llvm_gen_be.cpp` (644) | 1,190 | `LLVMBackendState`, `ScopeCleanupStack`, LLVM 20/21 API shims, multi-part int macros, target triple & DataLayout init, TU orchestration, IR verifier, cleanup |
| **Error Percolation** | `llvm_gen_be_error.h` (253) | `llvm_gen_be_error.cpp` (349) | 602 | `[[nodiscard]] enum class llvm_gen_be_error_t` (40 codes), `llvm_gen_be_error_context_t`, EDG `pos_st` source location decoding, `LLVM_GEN_BE_CHECK` macros |
| **Type & TBAA Lowering** | `llvm_gen_be_type.h` (212) | `llvm_gen_be_type.cpp` (959) | 1,171 | `a_type_ptr` -> `llvm::Type*`, `_BitInt(N)`, FP16/BF16/FP80/FP128/PPC_FP128, opaque `ptr` address spaces, cyclic struct breaking, bitfield storage coalescing, TBAA metadata |
| **Constant Evaluator** | `llvm_gen_be_const.h` (222) | `llvm_gen_be_const.cpp` (1,167) | 1,389 | `evaluate_constant`: 128-bit host/multi-part ints, SoftFloat -> `APFloat`, aggregates, bitfield byte packing, `ck_init_repeat`, sparse arrays, `ck_address`, `blockaddress` diffs |
| **Declarations & Modules** | *(in `llvm_gen_be_internal.h`)* | `llvm_gen_be_decl.cpp` (3,118) | 3,118 | Two-pass TU lowering, tentative array RAUW upgrade, Itanium/MSVC mangling helpers, TLS/COMDAT/alias/`ifunc`/`weakref`, `llvm.global_ctors`/`dtors`, `__cxa_guard_*`, C++20 `_ZGIW` |
| **Expressions** | `llvm_gen_be_expr.h` (242) | `llvm_gen_be_expr.cpp` (4,697) | 4,939 | Lvalue/Rvalue lowering, bitfield RMW, C11/C++11 atomics, checked overflow arithmetic, `__builtin_clear_padding`, complex math, vector shuffles, call & ABI argument marshalling |
| **Statements & Control Flow** | *(in `llvm_gen_be_internal.h`)* | `llvm_gen_be_stmt.cpp` (1,469) | 1,469 | Iterative `stmt->next` loop, `if`/`while`/`do`/`for`/`switch`, Duff's device, computed goto (`indirectbr`), GNU `({ ... })`, VLA `stacksave`/`stackrestore`, RAII scope exits |
| **Target SIMD Intrinsics** | `llvm_gen_be_intrinsics.h` (153) | `llvm_gen_be_intrinsics.cpp` (557) | 710 | Architecture-specific builtins for x86 SSE/AVX/AVX-512, AArch64 NEON/SVE, RISC-V Vector (RVV), and WebAssembly SIMD128 |
| **VTables & Thunks** | `llvm_gen_be_vtable.h` (218) | `llvm_gen_be_vtable.cpp` (1,159) | 1,377 | Itanium `_ZTV`, VTT (`_ZTT`), construction vtables (`_ZTC`), `this`/covariant thunks, key-function linkage; MSVC `??_7` VFTable and `??_8` VBTable |
| **RTTI & `dynamic_cast`** | `llvm_gen_be_rtti.h` (86) | `llvm_gen_be_rtti.cpp` (738) | 824 | Itanium `_ZTI`/`_ZTS` (`__fundamental`/`__class`/`__si_class`/`__vmi_class`/`__pointer`/`__pointer_to_member_type_info`), MSVC `??_R0`–`??_R4`, `dynamic_cast`, `typeid` |
| **Pointers-to-Members** | `llvm_gen_be_ptrmem.h` (98) | `llvm_gen_be_ptrmem.cpp` (596) | 694 | Data member offsets (`-1` null sentinel on Itanium), member function `{ptr, adj}` (x86_64 vs ARM64 odd-`adj` vs MSVC inheritance layouts), virtual call & base casts |
| **Exception Handling & SEH** | `llvm_gen_be_eh.h` (164) | `llvm_gen_be_eh.cpp` (1,072) | 1,236 | Itanium `invoke`/`landingpad`/`__gxx_personality_v0`/`__cxa_*` + Windows `__CxxFrameHandler3`/`__C_specific_handler` (`catchswitch`/`catchpad`/`cleanuppad`, `localescape`, `/EHa`) |
| **C++20 Coroutines** | `llvm_gen_be_coro.h` (117) | `llvm_gen_be_coro.cpp` (673) | 790 | Switch-resumed `llvm.coro.{id,alloc,size,begin,save,suspend,end,free,promise,subfn.addr,noop}`, ramp/suspend/final-suspend, symmetric transfer |
| **OpenMP 4.5 / 5.x & Offload** | `llvm_gen_be_omp.h` (353) | `llvm_gen_be_omp.cpp` (2,164) | 2,517 | Microtask outlining (`__kmpc_fork_call`), `for`/`sections`/`single`/`master`/`critical`/`barrier`/`task`/`simd`/`atomic`/`flush`, GPU `nvptx64`/`amdgcn` `__tgt_target_kernel` |
| **Inline Assembly** | `llvm_gen_be_asm.h` (85) | `llvm_gen_be_asm.cpp` (509) | 594 | GNU extended `asm` & `asm goto` (`callbr`), constraint canonicalization, multi-output struct unpacking, MSVC `__asm` Intel-dialect parser/translator |
| **Debug Info (DWARF/CV)** | `llvm_gen_be_debug.h` (523) | `llvm_gen_be_debug.cpp` (389)<br>`llvm_gen_be_debug_scopes.cpp` (243)<br>`llvm_gen_be_debug_types.cpp` (955) | 2,110 | `DIBuilder`, DWARF v4/v5, Split DWARF (`.dwo`), Windows CodeView, cyclic `DICompositeType` forward-decl resolution, template-isolated scope stack, macros |
| **Optimization Pipeline** | `llvm_gen_be_opt.h` (76) | `llvm_gen_be_opt.cpp` (252) | 328 | New Pass Manager (`PassBuilder`), `-O0`..`-Oz`, instrumentation & sample PGO, ThinLTO/FullLTO pipelines, ASan/TSan/MSan/HWASan/UBSan/SafeStack/ShadowCallStack |
| **Machine Code Emission** | `llvm_gen_be_codegen.h` (174) | `llvm_gen_be_codegen.cpp` (372) | 546 | Target initialization, thread-safe `TargetMachine` cache, `.o` / `.s` / `.ll` / `.bc` / `.thinlto.bc` / `.dwo` emission |
| **Toolchain & LTO Driver** | `llvm_gen_be_driver.h` (112) | `llvm_gen_be_driver.cpp` (456) | 568 | Multi-TU tracking, in-process `llvm::lto::LTO`, system linker discovery & invocation (`ld`, `ld64`, `lld`, `link.exe`, CRT/unwind flags) |
| **ABI: System V AMD64** | `llvm_gen_be_abi_sysv_x86_64.h` (234) | `llvm_gen_be_abi_sysv_x86_64.cpp` (833) | 1,067 | 8-class eightbyte lattice, 6 GPR / 8 XMM tracking, `sret`/`byval`, `[1 x {i32, i32, ptr, ptr}]` `va_arg` CFG, top-level 12-ABI dispatcher |
| **ABI: ARM64 AAPCS64** | `llvm_gen_be_abi_aapcs64.h` (198) | `llvm_gen_be_abi_aapcs64.cpp` (738) | 936 | 1–4 lane HFA/HVA detection, X0–X7 / V0–V7, >16B indirect ptr, Linux vs Darwin `va_list`, Windows ARM64EC thunk signatures |
| **ABI: Windows x64** | `llvm_gen_be_abi_win64.h` (133) | `llvm_gen_be_abi_win64.cpp` (420) | 553 | 4-slot positional RCX/XMM0..R9/XMM3, {1,2,4,8}B direct coercion, `__vectorcall` HVA, `char*` `va_list` |
| **ABI: RISC-V 32/64** | `llvm_gen_be_abi_riscv64.h` (114) | `llvm_gen_be_abi_riscv64.cpp` (408) | 522 | LP64D / ILP32D hardware FP struct flattening (`{fp,fp}`, `{fp,int}`, `{int,fp}`), `a0–a7` / `fa0–fa7`, >`2*XLEN` indirect |
| **ABI: MIPS O32/N64** | `llvm_gen_be_abi_mips.h` (149) | `llvm_gen_be_abi_mips.cpp` (387) | 536 | MIPS32 O32 & MIPS64 N64 GPR/FPR slot allocation, big/little-endian aggregate shifting |
| **ABI: LoongArch64** | `llvm_gen_be_abi_loongarch64.h` (140) | `llvm_gen_be_abi_loongarch64.cpp` (377) | 517 | LP64D GAR (`$a0–$a7`) & FAR (`$fa0–$fa7`) FP/int struct flattening and `va_arg` |
| **ABI: PowerPC64 ELFv2** | `llvm_gen_be_abi_ppc64.h` (143) | `llvm_gen_be_abi_ppc64.cpp` (371) | 514 | Up-to-8-lane HFA/HVA, `r3–r10` parameter array, 16B return vs 64B param thresholds |
| **ABI:SystemZ (`s390x`)** | `llvm_gen_be_abi_s390x.h` (148) | `llvm_gen_be_abi_s390x.cpp` (294) | 442 | `%r2–%r6` (5 GPRs), `%f0/%f2/%f4/%f6`, 1-field float struct promotion, indirect reference |
| **ABI: ARM32 AAPCS-VFP** | `llvm_gen_be_abi_arm32.h` (91) | `llvm_gen_be_abi_arm32.cpp` (336) | 427 | `r0–r3` 32-bit word packing, double-word even-register alignment, VFP `s0–s15`/`d0–d7` HFA |
| **ABI: SPARC V8/V9** | `llvm_gen_be_abi_sparc.h` (142) | `llvm_gen_be_abi_sparc.cpp` (283) | 425 | Register windows (`%o0–%o5`), SPARC64 V9 up-to-32B struct register return & FP slots |
| **ABI: WebAssembly** | `llvm_gen_be_abi_wasm.h` (88) | `llvm_gen_be_abi_wasm.cpp` (223) | 311 | Wasm32/Wasm64 Basic C ABI: single-leaf scalar unwrapping vs linear-memory `byval`/`sret` |
| **ABI: x86-32** | `llvm_gen_be_abi_x86_32.h` (90) | `llvm_gen_be_abi_x86_32.cpp` (204) | 294 | `cdecl`, `stdcall`, `fastcall`, `thiscall`, `vectorcall`, `inreg` (`regparm`), Darwin/MSVC small-struct return |

### 1.3 Complete File Inventory: GCC (`libgccjit`) Backend (`src/gcc_gen_be_*` — 32 Files, 26,713 LOC + Runtime)

| Subsystem | Header File (LOC) | Implementation File (LOC) | Combined LOC | Core Responsibility |
| :--- | :--- | :--- | ---: | :--- |
| **Main Orchestrator** | `gcc_gen_be_main.h` (138) | `gcc_gen_be_main.c` (854) | 992 | `gcc_gen_be()`, `gcc_gen_be_early_init()`, option wiring (`-O0..Ofast`, `-g`, `-fPIC`, `-march`), multi-TU driver, C++20 `_ZGIW` module init, `gcc_jit_context_compile_to_file` |
| **Error Percolation** | `gcc_gen_be_error.h` (368) | `gcc_gen_be_error.c` (315) | 683 | `GCC_GEN_BE_NODISCARD enum gcc_gen_be_error` (82 codes), `GCC_GEN_BE_NOEXCEPT`, `GCC_GEN_BE_CHECK` & `GCC_GEN_BE_CHECK_CLEANUP` macros, EDG diagnostic bridge |
| **Dynamic `libgccjit` Loader** | `gcc_gen_be_lib_loader.h` (567) | `gcc_gen_be_lib_loader.c` (606) | 1,173 | Windows `LoadLibraryA`/`GetProcAddress` macro indirection (`p_gcc_jit_*`) & POSIX `dlopen`/`dlsym` capability probing across GCC 10–15+ (`sizeof`, `bitcast`, vectors, constructors, GIMPLE EH) |
| **Context & State Stacks** | `gcc_gen_be_context.h` (520) | `gcc_gen_be_context.c` (805) | 1,325 | `gcc_gen_be_context_t`, `break`/`continue`/`switch`/`cleanup`/`landing_pad` stacks, `gcc_gen_be_seh_finally_entry_t`, `gcc_gen_be_coro_state_t`, `gcc_gen_be_eh_state_t` |
| **8-Domain Hash Cache** | `gcc_gen_be_cache.h` (117) | `gcc_gen_be_cache.c` (306) | 423 | Open-chaining hash maps (`TYPE`, `VAR`, `FUNC`, `LABEL`, `FIELD`, `SWITCH_CASE`, `GUARD`, `STRING`), 0.75 load factor resizing, hit/miss telemetry |
| **Source Locations** | `gcc_gen_be_location.h` (59) | `gcc_gen_be_location.c` (146) | 205 | EDG `a_source_position` (`seq` -> file/line/column via `conv_seq_to_file_and_line`) -> `gcc_jit_location*` |
| **Types, Decimal & Fixed-Pt** | `gcc_gen_be_type.h` (217) | `gcc_gen_be_type.c` (1,242) | 1,459 | Topological `deferred_struct_entry_t[4096]` cycle breaking, EBO/tail-padding layout, `__bf_container_*` bitfield coalescing, IEEE 754 BID/DPD `_Decimal32/64/128`, ISO `_Fract`/`_Accum` |
| **Target ABIs & Attributes** | `gcc_gen_be_abi.h` (494) | `gcc_gen_be_abi.c` (1,775) | 2,269 | 9 target ABIs (`SYSV_AMD64`, `AAPCS64`, `DARWIN_ARM64`, `WIN64`, `RISCV64_LP64D`, `SYSV_I386`, `PPC64LE_ELFV2`, `S390X`, `AAPCS32`), register coercion, `va_arg` CFG, attributes |
| **Declarations & Linkage** | `gcc_gen_be_decl.h` (131) | `gcc_gen_be_decl.c` (1,249) | 1,380 | Functions, globals, TLS models, custom sections, weak/COMDAT assembly directives, static initializers, C++20 imported module decls |
| **Global Ctors / `__dso_handle`** | `gcc_gen_be_dtor.h` (50) | `gcc_gen_be_dtor.c` (112) | 162 | `__cxa_atexit` registration and DSO-safe `__dso_handle` resolution |
| **Expressions & Constants** | `gcc_gen_be_expr.h` (193) | `gcc_gen_be_expr.c` (5,305) | 5,498 | Lvalue/Rvalue lowering, 128-bit ints, bitfield RMW & constant word packing, dynamic init, `new[]`/`delete[]` cookies, ptr-to-member, complex & SIMD vector ops |
| **Statements, Coro & Gotos** | `gcc_gen_be_stmt.h` (282) | `gcc_gen_be_stmt.c` (3,515) | 3,797 | Iterative stmt loop, switch range dedup, computed-goto switch synthesis, VLA stack save/restore, GNU `asm`/`asm goto` constraint splitting, SEH `__try`/`__finally`, **explicit C++20 coroutine frame/ramp/resume/destroy & HALO** |
| **Builtins & Atomics** | `gcc_gen_be_builtin.h` (92) | `gcc_gen_be_builtin.c` (1,171) | 1,263 | `__atomic_*` / `__sync_*`, overflow arithmetic, math/bit/string builtins, `__builtin_clear_padding`, x86/ARM SIMD builtin mapping |
| **Exception Handling & Tables** | `gcc_gen_be_eh.h` (380) | `gcc_gen_be_eh.c` (2,669) | 3,049 | Dual-mode EH: runtime GIMPLE EH probing + DWARF `.gcc_except_table` LSDA ULEB128/SLEB128 encoder + Win64 `.pdata`/`.xdata` + ARM EHABI `.ARM.extab` + SJLJ frame emission |
| **Hybrid EH Runtime Bridge** | *(in `lib_src/`)* | `lib_src/gcc_be_eh_rt.c` (249) | 249 | Thread-local `gcc_be_eh_frame_t` SJLJ stack, `__cxa_throw` interception, recursive Itanium RTTI inheritance/vbase/pointer matcher (`gcc_be_match_abi_type_info`), `RTLD_NEXT` fallback |
| **RTTI, VTables & Mangler** | `gcc_gen_be_rtti.h` (192) | `gcc_gen_be_rtti.c` (2,128) | 2,320 | Itanium C++ ABI mangler with `S_`/`S0_` substitution table, `_ZTV`/`_ZTT`/`_ZTC` vtables, `_ZTI`/`_ZTS` RTTI globals, `this`/covariant tail-call thunks, `__dynamic_cast` |
| **Debug & DWARF Assembly** | `gcc_gen_be_debug.h` (186) | `gcc_gen_be_debug.c` (529) | 715 | Lexical block scopes, custom `.debug_info`/`.debug_abbrev` DWARF DIE emission for classes, base offsets, virtual methods, enums, and template parameters |

---

## 2. Multi-Backend Integration into the EDG Front End (`cpfe`)

### 2.1 End-to-End Compilation Pipeline and the Asymmetric IL Lowering Boundary

The central architectural insight enabling both native backends to achieve full C++23 language coverage without duplicating 74,000+ lines of front-end semantics is **selective reuse of EDG IL Lowering (`src/lower_il.c`, `src/lower_init.c`, `src/lower_name.c`) coupled with suppression of portable EH lowering (`DO_FULL_PORTABLE_EH_LOWERING = FALSE`)**.

```
                      ┌──────────────────────────────────────────────────┐
                      │   C89–C23 / C++98–C++23 / GNU / MSVC Source TU   │
                      └────────────────────────┬─────────────────────────┘
                                               │
                                               ▼
                      ┌──────────────────────────────────────────────────┐
                      │   EDG Front End (Lexer, Parser, Overload,        │
                      │   Templates, Constexpr Interpreter, C++20 BMI)   │
                      └────────────────────────┬─────────────────────────┘
                                               │ High-Level EDG IL (file_scope_region)
                          ┌────────────────────┴────────────────────┐
                          │ DO_IL_LOWERING == 0                     │ DO_IL_LOWERING == 1
                          ▼                                         ▼
           ┌──────────────────────────────┐          ┌──────────────────────────────────────────┐
           │   cp_gen_be (cpfe-cp)        │          │   EDG IL Lowering (lower_il, lower_init) │
           │   Unlowered C++ Source Gen   │          │   • Class layout, EBO & vtable tables    │
           └──────────────────────────────┘          │   • Ctor/dtor/temporary expansion        │
                                                     │   • Itanium / MSVC name mangling         │
                                                     └──────────────────┬───────────────────────┘
                                                                        │
                                ┌───────────────────────────────────────┴───────────────────────────┐
                                │ DO_FULL_PORTABLE_EH_LOWERING == 1                                 │ DO_FULL_PORTABLE_EH_LOWERING == 0
                                ▼                                                                   ▼
                 ┌──────────────────────────────┐                        ┌──────────────────────────────────────────────────────┐
                 │   c_gen_be (--c-be)          │                        │   Preserved Structured Native Constructs in IL:      │
                 │   Portable setjmp/longjmp C  │                        │   • stmk_try_block, enk_throw, a_handler_ptr         │
                 │   (.int.c -> host C compiler)│                        │   • stmk_coroutine, a_coroutine_descr_ptr            │
                 └──────────────────────────────┘                        │   • High-level RTTI / vtable supplement graphs       │
                                                                         │   • OpenMP pragmas, GNU asm goto, MSVC __try/__asm   │
                                                                         └──────────────────────────┬───────────────────────────┘
                                                                                                    │ Runtime CLI Dispatch (src/cfe.c)
                                                                          ┌─────────────────────────┴─────────────────────────┐
                                                                          │ !gcc_be_enabled                                   │ gcc_be_enabled
                                                                          ▼                                                   ▼
                                                       ┌─────────────────────────────────────┐             ┌─────────────────────────────────────┐
                                                       │   LLVM IR Backend (llvm_gen_be)     │             │   GCC Backend (gcc_gen_be)          │
                                                       │   • Direct C++17 LLVM SSA IR        │             │   • Dynamic dlopen/LoadLibrary C API│
                                                       │   • 12 Target ABIs, TBAA, CoroSplit │             │   • 9 Target ABIs, Typed GIMPLE     │
                                                       │   • DWARF / Win64 SEH Funclets      │             │   • Dual-Mode EH (GIMPLE / SJLJ+ABI)│
                                                       │   • PassBuilder, PGO, LTO, Offload  │             │   • Explicit Coro State Machine/HALO│
                                                       └──────────────────┬──────────────────┘             └──────────────────┬──────────────────┘
                                                                          │                                                   │
                                                                          ▼                                                   ▼
                                                       ┌─────────────────────────────────────┐             ┌─────────────────────────────────────┐
                                                       │ .ll / .bc / .s / .o / Executable    │             │ .s / .o / .so / Executable          │
                                                       └─────────────────────────────────────┘             └─────────────────────────────────────┘
```

#### Why `DO_IL_LOWERING = 1` + `DO_FULL_PORTABLE_EH_LOWERING = 0` Is Optimal

In legacy EDG configurations (`src/targ_def.h:5044`), `c_gen_be` sets `DO_FULL_PORTABLE_EH_LOWERING = TRUE`. In that mode, `src/lower_eh.c` (6,675 LOC) lowers every C++ `try`/`catch`, `throw`, and RAII scope variable into explicit `setjmp` buffers, integer region indices, and linked-list cleanup descriptors suitable for K&R/ANSI C transpilation. While portable, that transformation **destroys zero-cost unwinding semantics** and prevents ABI interoperability with precompiled system `libstdc++`, `libc++`, or MSVC `vcruntime` binaries.

By setting in `src/targ_def.h:5047`:
```c
#ifndef DO_FULL_PORTABLE_EH_LOWERING
#if BACK_END_IS_LLVM_GEN_BE || BACK_END_IS_GCC_GEN_BE
#define DO_FULL_PORTABLE_EH_LOWERING FALSE
#else
#define DO_FULL_PORTABLE_EH_LOWERING TRUE
#endif
#endif
```
both `llvm_gen_be` and `gcc_gen_be` receive the best of both worlds:
- **Resolved by `lower_il` / `lower_init` / `lower_name`**: Overload resolution, template instantiation, `constexpr` evaluation, implicit conversions, base-class subobject offsets (`a_base_class`), constructor vptr initialization sequences, destructor ordering, and base Itanium/Microsoft symbol mangling.
- **Preserved intact for the native backends**: `stmk_try_block` statements with typed `a_handler` chains, `enk_throw` expressions, `a_dynamic_init` destruction lifetimes (`dip->variable`), `stmk_coroutine` / `stmk_coroutine_return` + `a_coroutine_descr`, `stmk_microsoft_try`, OpenMP pragma bindings, and class RTTI/vtable inheritance graphs (`a_class_type_supplement`).

### 2.2 Preprocessor Configuration Lattice (`defines.h`, `host_envir.h`, `targ_def.h`, `lang_feat.h`)

To allow `BACK_END_IS_LLVM_GEN_BE` and `BACK_END_IS_GCC_GEN_BE` to be enabled independently or simultaneously without breaking hundreds of legacy `#if BACK_END_IS_C_GEN_BE` guards across `src/`, `src/defines.h` and `src/host_envir.h` enforce strict compile-time lattice invariants:

1. **Isolation of `cpfe-cp` (`BACK_END_IS_CP_GEN_BE`)**: Both `src/CMakeLists.txt` and `src/cmake/cpfe-cp/CMakeLists.txt` share the root macro definitions, but `cpfe-cp` compiles with `CP_GEN_BE_VERSION` (`BACK_END_IS_CP_GEN_BE = 1`, `DO_IL_LOWERING = 0`). `src/host_envir.h:524–532` forcibly undefines `BACK_END_IS_GCC_GEN_BE` and `BACK_END_IS_LLVM_GEN_BE` whenever `BACK_END_IS_CP_GEN_BE` is true, guaranteeing `cpfe-cp` never links or calls native backend routines.
2. **Suppression of Default `BACK_END_IS_C_GEN_BE`**: Historically, `host_envir.h` defaulted `BACK_END_IS_C_GEN_BE` to `TRUE` whenever `BACK_END_IS_CP_GEN_BE` was `FALSE`. In `multibackend`, `BACK_END_IS_C_GEN_BE` is forced to `0` whenever `BACK_END_IS_LLVM_GEN_BE || BACK_END_IS_GCC_GEN_BE` is enabled (`defines.h:314`, `host_envir.h:542`), preventing duplicate symbol definitions of `back_end(void)` in `c_gen_be.c`.
3. **On-Demand Template Instantiation (`defines.h:3743`)**: When `BACK_END_IS_LLVM_GEN_BE` is active, `DEFAULT_INSTANTIATION_MODE` is overridden to `tim_used` (rather than `tim_none` or multi-file `.ii` deferred instantiation), ensuring all transitively referenced function templates and virtual member functions are instantiated within the active translation unit before LLVM IR emission.
4. **Zero-Cost Exceptions Enabled by Default (`lang_feat.h:194`)**: `DEFAULT_EXCEPTIONS_ENABLED` is changed from `BACK_END_IS_CP_GEN_BE`-only to `TRUE` whenever any of `CP_GEN_BE`, `LLVM_GEN_BE`, or `GCC_GEN_BE` is enabled.
5. **Lexical Promotion & VLA Prototype Rules for GCC (`targ_def.h:4868, 4910`)**: `PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE` and `NO_VLA_DIMENSION_TEMPORARIES_IN_FUNCTION_PROTOTYPES` are enabled for `BACK_END_IS_GCC_GEN_BE` just like `BACK_END_IS_C_GEN_BE`, ensuring function-local types and static variables have file-scope lifetimes compatible with `libgccjit`'s top-level struct/global APIs.

### 2.3 Translation Unit Dispatch and Multi-TU Orchestration (`src/cfe.c`, `src/fe_wrapup.c`)

In `src/cfe.c`, the per-translation-unit compilation loop (`do { process_translation_unit(...); ... } while (get_next_source_file());`) was extended with three hooks:

1. **Early Initialization (`cfe.c:115`)**: When `MAKE_FRONT_END_CALLABLE` is enabled alongside `BACK_END_IS_GCC_GEN_BE`, `gcc_gen_be_early_init()` resets global backend contexts prior to timer and CLI parsing so repeated in-process library invocations start cleanly.
2. **Dual-Backend Runtime Selection (`cfe.c:186–222`)**:
   - In **Mode D** (`BACK_END_IS_LLVM_GEN_BE && BACK_END_IS_GCC_GEN_BE`), `cfe.c` checks the thread-local flag `gcc_be_enabled` (set by `--gcc` or `--backend=gcc`): if true, it invokes `gcc_gen_be()`; otherwise it defaults to `llvm_gen_be()`.
   - In **Mode B** (`LLVM` only) or **Mode C** (`GCC` only), it invokes the compiled-in native backend directly and translates any non-OK `llvm_gen_be_error_t` / `gcc_gen_be_error_t` into EDG's `diagnostic_counters.total.errors`.
   - In **Mode A** (neither native backend enabled), it falls through to legacy `back_end()`.
3. **Post-Loop Multi-TU Link Finalization (`cfe.c:252–268`)**: When compiling multiple translation units in a single command (`cpfe a.cpp b.cpp -o app`), each iteration of the `do-while` loop emits an object file (or bitcode module under `-flto`) and registers its temporary artifact path in `llvm_gen_be_driver.cpp`. After `get_next_source_file()` returns `FALSE` and `most_severe_diagnostic < es_error`, `cfe.c` invokes `llvm_finalize_multi_tu_link()` to run either in-process `llvm::lto::LTO` or the platform system linker across all accumulated TUs.
4. **Wrapup Memory Release (`fe_wrapup.c:908`)**: `llvm_gen_be_cleanup()` destroys `be_state` and releases the thread-safe `TargetMachine` cache.

### 2.4 Catalogue of All 28 Modified Legacy Front-End Files in `src/`

Every single edit to pre-existing files in `src/` was driven by a concrete multi-backend requirement or a latent front-end bug exposed by native ABI/codegen verification:

| File(s) | Lines Changed | Root Cause & Architectural Purpose |
| :--- | ---: | :--- |
| `src/cfe.c`, `src/fe_wrapup.c` | +76 / -2 | Multi-backend `#include`s, `gcc_gen_be_early_init()`, runtime `gcc_be_enabled` vs `llvm_gen_be()` dispatch, `llvm_finalize_multi_tu_link()`, `llvm_gen_be_cleanup()` |
| `src/cmd_line.h`, `src/cmd_line.c` | +823 / -11 | 54 new `an_option_kind` enumerators (`--backend`, `--llvm*`, `--gcc*`, `-flto`, `-fprofile-*`, `-fsanitize`, `-fstack-protector*`, `-g`, `-O0..-Ofast`, `-fPIC`, `-march`, etc.) |
| `src/defines.h`, `src/host_envir.h`, `src/host_envir.c` | +52 / -45 | Backend macro lattice, `tim_used` default instantiation, `use_predefined_macro_file = FALSE`, replacing monolithic `<windows.h>` with modular Win32 API headers (`<windef.h>`, `<libloaderapi.h>`, etc.) |
| `src/targ_def.h`, `src/float_pt.c` | +22 / -11 | `DO_FULL_PORTABLE_EH_LOWERING = FALSE`; renaming Berkeley SoftFloat `float32_t`/`float64_t` to `edg_softfloat32_t`/`edg_softfloat64_t` to eliminate typedef collisions with ARM64 `<arm_neon.h>` |
| `src/types.c` | +4 / -2 | **Critical C ABI Fix**: In C mode (`C_mode()`), `symbol_for(tp)` is `NULL` for struct types, causing `is_trivially_copyable()` to return `FALSE` and misclassify plain C structs as non-trivially copyable (passing via hidden pointer instead of registers). Fixed to inspect virtual function/base flags directly |
| `src/sys_predef.c` | +5 / -1 | **Darwin ARM64 `va_list` Fix**: Suppresses `struct __va_list` tag synthesis on `__APPLE__ && __aarch64__` where Apple's ABI defines `__builtin_va_list` as `char*` |
| `src/lexical.c` | +27 / -0 | **Decimal Float Precision Fix**: Preserves raw ASCII lexemes (`< 64` bytes) for `tok_float_constant` in IL memory so `_Decimal32`/`_Decimal64`/`_Decimal128` literals (`0.1dd`) avoid binary FP rounding before BID encoding |
| `src/il_def.h`, `src/il_alloc.c`, `src/il_alloc.h`, `src/il.h`, `src/il.c` | +30 / -12 | Adds `module_info` & `is_exported` on `a_source_correspondence` (C++20 module mangling), `virtual_function_number` on `a_routine` (GCC DWARF vtable slot), `bit_field_alignment_type` & `pk_pack` alignment for GCC, and null-guards `parent_class_of(rp)` on non-member `operator=` in `il.c` |
| `src/layout.c`, `src/inline.c`, `src/lower_il.c` | +12 / -6 | Tracks `#pragma pack` alignment and `bit_field_alignment_type` for `BACK_END_IS_GCC_GEN_BE`; prevents inlining cast function-pointer calls in `inline.c` that violate `libgccjit` strict signature typing |
| `src/il_to_str.c`, `src/il_to_str.h`, `src/cp_gen_be.c`, `src/cp_gen_be.h`, `src/c_gen_be.c`, `src/c_gen_be.h` | +46 / -8 | Enables `form_asm_name`/`form_var_reg_name` for GCC backend; strips outer parentheses on `ak_availability` version tokens; guards legacy `back_end()` symbol when `BACK_END_IS_LLVM_GEN_BE` is active |
| `src/ms_metadata.cpp`, `src/error.h`, `src/lang_feat.h` | +31 / -8 | Modular Win32 headers before `<cor.h>`/`<metahost.h>`; `EDG_GCC_BE_ISOLATED_TEST` standalone unit-test shim in `error.h`; `DEFAULT_EXCEPTIONS_ENABLED = TRUE` in `lang_feat.h` |

---

## 3. Build System, Macro Configuration (`macro-conf`), and Portability Layer

### 3.1 The 4-Mode CMake Matrix (`CMakeLists.txt`, `cmake/macro-conf.cmake`)

Top-level `CMakeLists.txt` introduces two orthogonal options (defaulting to `ON` in `CMakePresets.json` multi-backend presets, with backward-compatible aliasing from `ENABLE_LLVM_BACKEND`):
- `EDG_ENABLE_LLVM_BACKEND`: Discovers `LLVMConfig.cmake` (probing standard paths, Homebrew `/opt/homebrew/opt/llvm`, and `/usr/lib/llvm-{22..16}/lib/cmake/llvm` on Linux x86_64/ARM64), links native targets (`X86`, `AArch64`, `ARM`, `RISCV`, `LoongArch`, `Mips`, `PowerPC`, `SystemZ`, `Sparc`, `WebAssembly`, `NVPTX`, `AMDGPU`) plus `Passes`, `Coroutines`, `IPO`, `Instrumentation`, `LTO`, `BitWriter`, `IRReader`, `Linker`, and sets `BACK_END_IS_LLVM_GEN_BE=1`.
- `EDG_ENABLE_GCC_BACKEND`: Invokes `find_package(LibGCCJIT REQUIRED)` and sets `BACK_END_IS_GCC_GEN_BE=1`.

| Build Mode | `EDG_ENABLE_LLVM_BACKEND` | `EDG_ENABLE_GCC_BACKEND` | Compiled Backends in `cpfe` | Default Backend | Runtime Override Flags |
| :--- | :---: | :---: | :--- | :--- | :--- |
| **Mode A (Legacy)** | `OFF` | `OFF` | `c_gen_be` (and `cp_gen_be` in `cpfe-cp`) | `c_gen_be` | `--c-be` |
| **Mode B (LLVM)** | `ON` | `OFF` | `llvm_gen_be` | `llvm_gen_be` | `--llvm`, `--backend=llvm` |
| **Mode C (GCC)** | `OFF` | `ON` | `gcc_gen_be` | `gcc_gen_be` | `--gcc`, `--backend=gcc` |
| **Mode D (Dual Native)** | `ON` | `ON` | **Both `llvm_gen_be` and `gcc_gen_be`** | `llvm_gen_be` | `--llvm` / `--backend=llvm` or `--gcc` / `--backend=gcc` |

`dev_tools/verify_backend_modes.py` automatically configures, builds, and smoke-tests all four modes in CI (`.github/workflows/strict-build-check.yml`), asserting both positive compilation (emitting valid LLVM IR / object files) and negative rejection of disabled backend flags.

### 3.2 Cross-Platform `FindLibGCCJIT.cmake` and MSVC Header Shadow Prevention

`cmake/FindLibGCCJIT.cmake` (132 LOC) solves a notorious cross-platform packaging problem:
1. **Header + Shared Library Discovery**: Searches `LIBGCCJIT_ROOT`, `GCC_JIT_INCLUDE_DIR`, Homebrew GCC kegs (`/opt/homebrew/Cellar/gcc/*`), Debian/Ubuntu GCC plugin directories (`/usr/lib/gcc/{x86_64,aarch64}-linux-gnu/{15..10}/include`), and MSYS2/MinGW64 (`C:/msys64/mingw64`).
2. **Windows MSVC Header-Only Mode**: Because `libgccjit-0.dll` is distributed by MinGW-w64 without an MSVC `.lib` import library, `src/gcc_gen_be_lib_loader.{h,c}` resolves every `gcc_jit_*` symbol dynamically at runtime via `LoadLibraryA` + `GetProcAddress`. Thus, on `WIN32`, `FindLibGCCJIT.cmake` requires **only `libgccjit.h`** at compile time!
3. **Preventing MinGW Header Shadowing under MSVC**: When `libgccjit.h` resides in `C:/msys64/mingw64/include` alongside MinGW's own `stdlib.h` and `stdint.h`, adding `-IC:/msys64/mingw64/include` to `cl.exe` fatally shadows MSVC's Universal CRT headers (`corecrt.h`). `FindLibGCCJIT.cmake:96–108` detects `MSVC && EXISTS "${LIBGCCJIT_INCLUDE_DIR}/stdlib.h"`, copies solely `libgccjit.h` (and `libgccjit++.h`) into an isolated `${CMAKE_BINARY_DIR}/libgccjit_inc_isolated` directory, and exposes only that clean directory to `LibGCCJIT::LibGCCJIT`.

### 3.3 Portable Linux Binary Compatibility Layer (`src/linux_libc_compat.c`)

When building `cpfe` on modern Linux distributions (Ubuntu 24.04+, Fedora 40+, Debian Trixie) with glibc 2.38+, `<stdlib.h>` transparently redirects `strtol`, `strtoul`, `strtoll`, and `strtoull` to `__isoc23_strtol*` symbols (`GLIBC_2.38`), rendering release binaries unrunnable on RHEL 8/9, Ubuntu 20.04/22.04, Debian 11/12, or Alpine Linux (`musl`). `src/linux_libc_compat.c` (173 LOC) provides:
1. **Weak C23 Integer Parser Wrappers (`__attribute__((weak)) __isoc23_strto*`)**: Pre-normalizes C23 `0b`/`0B` binary prefixes (when `base == 0 || base == 2`) while rejecting `0b` followed by non-binary digits (matching exact glibc 2.38 `endptr` semantics), then delegates via `dlsym(RTLD_NEXT, "strtol")` (or base-2 `strtol` fallback) without binding `GLIBC_2.38` symbol versions.
2. **Weak Itanium Static Guard Fallback (`__cxa_guard_acquire` / `release` / `abort`)**: Provides lock-free spin-atomic C++11 static-local initialization guards (`__atomic_load_n` / `__atomic_compare_exchange_n`) when linking minimal C targets without `libstdc++`/`libc++abi`.

---

## 4. Total Error Percolation & `noexcept` Reliability Architecture

A foundational design requirement across **all 59,429 lines** of the LLVM and GCC backends is **zero-exception, non-aborting, statically enforced error percolation**:
- Neither backend ever throws a C++ exception (`noexcept` / `GCC_GEN_BE_NOEXCEPT` on every internal and external function). Throwing across EDG's C-style `setjmp`/`longjmp` catastrophic error recovery (`src/error.c`) or across `libgccjit` C frames invokes undefined behavior.
- Neither backend uses `llvm::report_fatal_error`, `abort()`, or unchecked pointer dereferences on user input; every fallible operation returns a status enum and writes outputs via caller-supplied pointers (`T* out_val`).
- Both error enums carry **`[[nodiscard]]` at the type declaration** (`enum class [[nodiscard]] llvm_gen_be_error_t` and `enum GCC_GEN_BE_NODISCARD gcc_gen_be_error`), plus `-Werror=unused-result` in test builds, making it a **hard compile error** anywhere in the 95 backend files to ignore a call's return status.

### 4.1 LLVM Backend Error Subsystem (`src/llvm_gen_be_error.{h,cpp}`)

`llvm_gen_be_error_t` defines **40 status codes** (`0..39`), paired with `llvm_gen_be_error_context_t` (`error_code`, `file_name`, `line_number`, `column_number`, `a_seq_number seq_number`, and `char message[512]`). When an error occurs on an AST/IL node carrying an EDG `a_source_position`, `llvm_gen_be_format_diagnostic` resolves `pos.seq` via EDG's `conv_seq_to_file_and_line` into exact user file/line/column coordinates.

| Code | Enumerator | Code | Enumerator | Code | Enumerator |
| ---: | :--- | ---: | :--- | ---: | :--- |
| 0 | `ok` | 14 | `eh_lowering_failure` | 27 | `type_lowering_failed` |
| 1 | `invalid_argument` | 15 | `vtable_generation_failure` | 28 | `coro_lowering_failed` |
| 2 | `out_of_memory` | 16 | `coroutine_lowering_failure` | 29 | `omp_lowering_failed` |
| 3 | `unsupported_type` | 17 | `inline_asm_failure` | 30 | `eh_lowering_failed` |
| 4 | `unsupported_expr` | 18 | `internal_inconsistency` | 31 | `rtti_vtable_failed` |
| 5 | `unsupported_stmt` | 19 | `not_implemented` | 32 | `debug_info_failed` |
| 6 | `abi_classification_failed` | 20 | `omp_lowering_failure` | 33 | `opt_pipeline_failed` |
| 7 | `di_metadata_failure` | 21 | `offload_bundling_failed` | 34 | `codegen_emit_failed` |
| 8 | `pass_pipeline_failure` | 22 | `lto_indexing_failed` | 35 | `linker_invocation_failed` |
| 9 | `code_gen_failure` | 23 | `module_bmi_corrupt` | 36 | `gpu_offload_failed` |
| 10 | `verification_failure` | 24 | `dwarf_macro_emission_failed` | 37 | `module_io_failed` |
| 11 | `io_error` | 25 | `coro_frame_layout_failed` | 38 | `verification_failed` |
| 12 | `constant_eval_failure` | 26 | `null_pointer` | 39 | `internal_error` |
| 13 | `symbol_lookup_failure` | | | | |

Notably, `LLVM_GEN_BE_ASSERT_OK(expr)` (`[[maybe_unused]] const llvm_gen_be_error_t _ok_status = (expr);`) satisfies `[[nodiscard]]` on provably infallible calls with pre-validated non-null arguments without introducing dead branch-coverage edges under `llvm-cov`!

### 4.2 GCC Backend Error Subsystem (`src/gcc_gen_be_error.{h,c}`)

Because the GCC backend is written in C-idiomatic style (`malloc`/`free` manual heap lifetimes rather than C++ RAII containers), `gcc_gen_be_error.h` defines **82 fine-grained error codes** (`GCC_GEN_BE_SUCCESS = 0` through `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED = 81`) and two propagation macros:
- `GCC_GEN_BE_CHECK(expr)`: Early return when no heap buffers are live in the current stack frame.
- `GCC_GEN_BE_CHECK_CLEANUP(expr, err_var, cleanup_label)`: Stores the status code and jumps to a deterministic `cleanup:` block that frees `malloc`'d `gcc_jit_field**` / `gcc_jit_rvalue**` / `gcc_jit_case**` arrays before returning `err_var`, preventing memory leaks on error paths.

Categories across the 82 `gcc_gen_be_error_t` codes span dynamic loading (`LIBGCCJIT_LOAD_FAILED`, `LIBGCCJIT_SYMBOL_MISSING`), AST domains (`UNHANDLED_{TYPE,EXPR,STMT,DECL}`), ABI/coercion (`ABI_MISMATCH`, `ABI_CLASSIFICATION_FAILED`, `COERCION_FAILED`, `ABI_PROLOGUE_UNPACK_FAILED`, `VA_ARG_LOWERING_FAILED`), C++ object model (`VTABLE_*`, `VTT_*`, `THUNK_*`, `RTTI_*`, `MANGLING_*`, `COOKIE_ALLOCATION_FAILED`, `TAIL_CALL_THUNK_FAILED`), unwinding (`EH_*`, `CFI_UNWIND_FAILED`, `UNWIND_CLEANUP_FAILED`, `EH_CFG_EDGE_FAILED`, `LSDA_ENCODE_FAILED`), C++20 coroutines (`COROUTINE_{FRAME,RAMP,DISPATCH,PROMISE,SUSPEND,SPLIT}_FAILED`, `CORO_{TRANSFER,HALO}_FAILED`), numerics (`WIDE_INT_CONVERSION_FAILED`, `FLOAT_PRECISION_FAILED`, `FIXED_POINT_OVERFLOW`, `DECIMAL_FLOAT_UNSUPPORTED`, `DPD_FLOAT_CONVERSION_FAILED`), DWARF (`DWARF_{EMISSION,DIE_CREATION,TYPE_EMISSION,SCOPE,DIE,DIE_BINDING}_FAILED`), and C++20 modules (`MODULE_{IMPORT,EXPORT}_FAILED`).

---

## 5. Internals of the LLVM IR Backend (`src/llvm_gen_be*`)

### 5.1 Core State, Lifecycle, and Two-Pass Translation Unit Lowering

All per-translation-unit state in the LLVM backend is encapsulated inside a heap-allocated `LLVMBackendState` instance (`be_state` in `src/llvm_gen_be_internal.h:168`):
- **Core LLVM Handles**: `std::unique_ptr<llvm::LLVMContext> context`, `std::unique_ptr<llvm::Module> module`, and `std::unique_ptr<llvm::IRBuilder<>> builder`.
- **Memoization Maps**: `type_cache` (`a_type_ptr -> llvm::Type*`), `local_vars` (`a_variable_ptr -> llvm::Value*`), `reused_values` (`a_dynamic_init_ptr -> llvm::Value*` for EDG's `enk_reuse_value` DAG nodes), `vla_saved_stacks`, `array_to_vla_dim`, and `vla_dimension_values`.
- **Structured Control-Flow & RAII Stacks**: `break_blocks`, `continue_blocks`, `break_cleanup_depths`, `continue_cleanup_depths`, `current_landing_pads`, `landing_pad_cleanup_depths`, `active_funclet_pads` (Windows SEH funclet tokens), `seh_leave_blocks`, `stmt_expr_cleanup_depths` (GNU `({ ... })`), `label_blocks`, `label_cleanup_depths`, and `ScopeCleanupStack cleanup_stack`.

#### Two-Pass Translation Unit Lowering (`llvm_gen_be.cpp`, `llvm_gen_be_decl.cpp`)

`llvm_gen_be()` orchestrates module synthesis in a strict deterministic sequence:
1. **Triple & Exact `DataLayout` Resolution**: Resolves `--target=<triple>` (or `llvm::sys::getDefaultTargetTriple()`), queries `llvm::TargetRegistry::lookupTarget` via a cached `TargetMachine` to attach the exact hardware `llvm::DataLayout` (falling back to synthesized architectural DataLayout strings for cross-targets whose backend target library was not linked into LLVM).
2. **Debug State & Macro Emission**: If `llvm_debug_info_enabled`, initializes `llvm_gen_be_debug_state_t` (`DIBuilder`, `DICompileUnit`, `DIFile`, and DWARF v5 `DIMacro` definitions).
3. **Pass 1 — Global & Routine Forward Registration (`llvm_lower_file_scope_decls`)**:
   - Iterates `il_header.primary_scope->variables`, `nonstatic_variables`, and `routines`.
   - **Tentative Incomplete-to-Complete Global Array Upgrade**: In C (`int arr[]; ... int arr[4] = {1,2,3,4};`), a tentative declaration initially creates a `[0 x i32]` `GlobalVariable`. When the completed `[4 x i32]` definition is encountered, `llvm_gen_be_decl.cpp` allocates a new `GlobalVariable` with the complete `[4 x i32]` type, transfers linkage/alignment/section/visibility, replaces all prior ConstantExpr/instruction references via `old_gv->replaceAllUsesWith(new_gv)`, and erases `old_gv`.
   - Applies linkage (`internal`, `private`, `weak_odr`, `linkonce_odr`, `common`, `available_externally`, `extern_weak`), ELF/Mach-O visibility (`hidden`, `protected`), Windows `dllimport`/`dllexport`, TLS models (`generaldynamic`, `localdynamic`, `initialexec`, `localexec`), COMDAT groups (`llvm::Comdat::Any`), custom sections, `GlobalAlias` (`__attribute__((alias))` / `weakref`), and `GlobalIFunc` (`__attribute__((ifunc))`).
4. **Pass 2 — Polymorphic Tables, Function Bodies, Static Init, and C++20 Modules**:
   - Emits vtables (`_ZTV` / `??_7`), VTTs (`_ZTT`), and RTTI (`_ZTI` / `??_R*`) for polymorphic classes in `primary_scope->types`.
   - Lowers every defined routine body (`il_header.function_def_table[i].scope`).
   - Synthesizes `llvm.global_ctors` and `llvm.global_dtors` arrays sorted by `__attribute__((init_priority(N)))` (default `65535`), grouping namespace-scope dynamic initializers into `_GLOBAL__sub_I_<tu>` (and `__tls_init` for `thread_local` dynamic initializers).
   - **C++20 Modules (`llvm_emit_module_initializer`)**: For named module interface/partition TUs (`export module M;`), emits the ABI-standardized idempotent module initializer `_ZGIW<mangled_module>` guarded by an internal `i8` flag, chaining imported module `_ZGIW*` calls before executing local global constructors.
5. **IR Verification, Optimization, and Emission**: Finalizes `DIBuilder`, runs `llvm::verifyModule` (converting any broken IR invariant into `llvm_gen_be_error_t::verification_failure`), executes `run_optimization_pipeline`, and emits `.ll`, `.bc`, `.thinlto.bc`, `.s`, `.o`, `.dwo`, or linked executables.

### 5.2 Multi-Version LLVM C++ API Compatibility Layer (LLVM 16–23+)

LLVM's C++ internal headers break source compatibility every 6 months. The EDG LLVM backend compiles warning-free from **LLVM 16 through LLVM 23+** via seven targeted preprocessor shims across `llvm_gen_be_internal.h`, `llvm_gen_be_opt.cpp`, `llvm_gen_be_codegen.cpp`, and `llvm_gen_be_debug.cpp`:

| LLVM Version Boundary | Upstream LLVM C++ API Breaking Change | EDG Backend Compatibility Shim |
| :--- | :--- | :--- |
| **LLVM 15+ (Universal)** | Removal of typed pointers (`i32*` -> opaque `ptr`) | Backend exclusively uses `llvm::PointerType::get(*context, addr_space)` (`ptr`) and passes explicit pointee `llvm::Type*` to `CreateLoad`, `CreateGEP`, `CreateAlloca`, `byval(T)`, and `sret(T)` |
| **LLVM 17 -> 18** | `llvm::CodeGenOpt::Level` -> `llvm::CodeGenOptLevel`; `llvm::CGFT_ObjectFile` -> `llvm::CodeGenFileType::ObjectFile` | `llvm_codegen_opt_level_t` and `llvm_file_type_t` type aliases in `llvm_gen_be_codegen.cpp:25` |
| **LLVM 16 -> 17 -> 20+** | `llvm::PGOOptions` constructor changed from 6 args (LLVM 16) to 7 args adding `FSProfileFile` (LLVM 17+), while `PassBuilder` constructor removed `TargetLibraryInfoImpl` or shifted parameter arity across 16–20 | C++17 SFINAE / `std::is_constructible_v` compile-time trait dispatch in `llvm_gen_be_opt.cpp` selecting exact `PGOOptions` and `PassBuilder` overload arities without brittle patch-level checks |
| **LLVM 17 -> 18+** | `DIBuilder::createMethod` / `SPFlagArtificial` signature shifts | Conditional `DISPFlag` construction in `llvm_gen_be_debug.cpp` |
| **LLVM 19 -> 20** | `Intrinsic::getDeclaration` deprecated for `Intrinsic::getOrInsertDeclaration`; `BasicBlock::getTerminator()` asserts on open blocks; `getFirstInsertionPt()` returns `BasicBlock::iterator` vs `InstListType::iterator` | `EDG_LLVM_GET_INTRINSIC_DECL`, `EDG_LLVM_GET_TERMINATOR` (`getTerminatorOrNull()`), and `EDG_LLVM_FIRST_INSERTION_PT` (`getFirstNonPHIIt()`) in `llvm_gen_be_internal.h:50–63` |
| **LLVM 20 -> 21** | `Module::setTargetTriple(StringRef)` replaced by `Module::setTargetTriple(Triple)` | `EDG_LLVM_SET_TARGET_TRIPLE(mod, triple_str)` wrapping `llvm::Triple(triple_str)` on `LLVM_VERSION_MAJOR >= 21` (`llvm_gen_be_internal.h:43`) |

### 5.3 Type System, Bitfield Storage Synthesis, Address Spaces, and TBAA

`src/llvm_gen_be_type.cpp` (959 LOC) translates EDG `a_type_ptr` graphs (unwrapping `tk_typeref` qualifiers while retaining CV/restrict/address-space attributes where needed) into `llvm::Type*`:

1. **Arbitrary-Width & Extended Scalars**:
   - Standard integers (`tk_integer`) lower by exact bit size (`tp->size * 8`), while C23 `_BitInt(N)` inspects `tp->variant.integer.bit_int_bits` to construct exact `llvm::IntegerType::get(*ctx, N)` (`i1` through `i8388608`).
   - Floating-point (`tk_float`) maps `fk_float16` -> `half`, `fk_bfloat16` -> `bfloat`, `fk_float` -> `float`, `fk_double` -> `double`, `fk_float80` / 12-or-16-byte x87 `long double` -> `x86_fp80`, IEEE quad `fk_float128` / AArch64 `long double` -> `fp128`, and IBM double-double -> `ppc_fp128`.
2. **GPU & Heterogeneous Address Spaces (`llvm_resolve_target_address_space`)**:
   Maps named/numbered address space qualifiers (`__attribute__((address_space(N)))`, CUDA/HIP/OpenCL/SPIR-V `"global"`, `"shared"`, `"constant"`, `"local"`, `"generic"`) into target-specific `ptr addrspace(N)` values (e.g., `shared` -> `addrspace(3)` on NVPTX/AMDGPU, `local` -> `addrspace(5)` on NVPTX vs `addrspace(3)` on AMDGPU, `constant` -> `addrspace(4)` on NVPTX/AMDGPU vs `addrspace(2)` on SPIR-V).
3. **Cyclic Struct Breaking & Exact Byte-Offset Layout (`llvm_type_from_class_struct_union`)**:
   - Immediately creates an empty named `llvm::StructType::create(*ctx, name)` and inserts it into `be_state->type_cache[edg_type]` **before** lowering field types, breaking arbitrary self-referential cycles (`struct Node { Node* next; }`).
   - Rather than trusting LLVM's default struct field alignment to match EDG's C++ ABI layout (which fails on Itanium tail-padding reuse, `#pragma pack(N)`, empty base optimization, and virtual bases), the type builder walks EDG's computed byte offsets (`field->offset` and `base->offset`), inserting explicit `[pad x i8]` arrays for inter-field padding, truncating non-virtual base subobjects to `size_without_virtual_base_classes` when tail padding is reused by derived members, coalescing adjacent bitfields (`field->is_bit_field`) into `[N x i8]` or integer storage containers, and toggling `isPacked = true` whenever field alignment exceeds natural offset alignment.
   - Unions (`tk_union`) lower to `{ largest_or_most_aligned_member_ty, [tail_pad x i8] }` matching `edg_type->size` and `EDG_ALIGN_OF_TYPE(edg_type)`.
4. **Type-Based Alias Analysis (TBAA) Metadata (`llvm_get_tbaa_tag`)**:
   Constructs a standards-compliant LLVM TBAA tree rooted at `!"Simple C/C++ TBAA"`, anchored by `!"omnipotent char"` (allowing `char*`, `unsigned char*`, and `std::byte*` to alias all memory per `[basic.lval]`), `!"any pointer"`, distinct scalar nodes (`!"int"`, `!"long"`, `!"float"`, `!"double"`, enum types), and struct-path access tags `(StructNode, FieldTypeNode, ByteOffset)` attached to `LoadInst` and `StoreInst` at `-O1` and above.

### 5.4 Host-Independent Constant Evaluator (`src/llvm_gen_be_const.{h,cpp}`)

EDG can be configured with `INTEGER_VALUE_REPR_IS_A_HOST_INTEGER = 0` (the default in `cpfe` macro configurations supporting 128-bit target integers across 32/64-bit hosts), representing every `an_integer_value` as a **big-endian array of eight 16-bit unsigned parts** (`val.part[0..7]`, where `part[7]` is the least significant 16 bits and `part[0]` is the most significant 16 bits!).

In `llvm_gen_be_internal.h:66–88`, `EDG_INT_VAL_LOW64(val)` and `EDG_INT_VAL_HIGH64(val)` reconstruct the 128-bit integer into two `uint64_t` words fed into `llvm::APInt(bit_width, {low64, high64})`. (As detailed in Section 8.3, an early version wrote `#if ... && sizeof(an_integer_value) >= 16` in the preprocessor, where `sizeof` is unrecognized and evaluates to `0 == 0`, corrupting the upper 64 bits of negative 128-bit constants into `0` until replaced with compile-time `#if INT128_EXTENSIONS_ALLOWED || FIXED_POINT_ALLOWED`!).

`llvm_gen_be_const.cpp` (1,167 LOC) lowers all constant kinds:
- **Floating-Point (`ck_float`)**: Extracts exact IEEE bit patterns via EDG's `fetch_float_value` / SoftFloat routines into `llvm::APFloat` without host `double` truncation (preserving 80-bit `x86_fp80` and 128-bit `fp128` mantissas).
- **Aggregates (`ck_aggregate`)**:
  - *Structs/Classes*: Iterates lowered LLVM struct fields alongside EDG `a_constant_ptr` chains, packing adjacent bitfield constants via `llvm_pack_constant_bitfield_bytes` (shifting & OR-ing `con` bits at `(field->offset - container_offset)*8 + field->offset_bit_remainder` across arbitrary endianness) and emitting zeroed `[N x i8]` arrays for padding fields.
  - *Arrays*: Expands `ck_init_repeat` run-length nodes without blowing up compile time on zero-initialized mega-arrays (collapsing all-zero arrays directly to `llvm::ConstantAggregateZero::get(arr_ty)`).
  - *Unions*: Serializes active-member constant bytes plus zeroed tail padding when the initialized union member is smaller than the union's total `size`.
- **Addresses & Relocations (`ck_address`, `ck_label_difference`)**: Lowers `abk_routine`, `abk_variable`, `abk_constant` (string literals / backing constants), `abk_compound_literal`, and pointer arithmetic (`ptrtoint`/`getelementptr i8`), plus PIC jump-table label differences `&&target - &&base` via `ConstantExpr::getSub(ptrtoint(blockaddress(fn, target)), ptrtoint(blockaddress(fn, base)))`.

### 5.5 Target ABI Subsystem Across 12 Hardware Architectures

Why cannot a C/C++ front end simply emit `define %MyStruct @foo(%MyStruct %arg)` in LLVM IR? Because **LLVM IR does not implement C/C++ psABI aggregate classification**. Passing `%MyStruct = type { i32, float, double }` by value in raw LLVM IR causes LLVM's SelectionDAG/GlobalISel to split every scalar leaf into separate registers (`EDI`, `XMM0`, `XMM1`), violating the System V AMD64 ABI (which classifies byte 0..7 `{i32, float}` into a **single 64-bit `INTEGER` register `RDI`** because `INTEGER` dominates `SSE` within the same eightbyte, and byte 8..15 `{double}` into `XMM0`!).

Consequently, the EDG LLVM backend implements **12 dedicated target psABI engines** (`src/llvm_gen_be_abi_*.{h,cpp}`, 6,544 LOC) dispatched through `llvm_compute_function_abi_info(routine_ty, triple_str, &abi_info)`. Across **all** C++ targets, non-trivially copyable classes (having a non-trivial copy/move constructor or destructor per Itanium C++ ABI §3.1.1 / MSVC x64 rules) are intercepted first and passed by **invisible reference** (pointer in GPR/stack) and returned via hidden caller-allocated **`sret(T)`** buffer.

| Target Architecture | Module (`llvm_gen_be_abi_*`) | Arg GPRs | Arg FPR/SIMD | Aggregate Register Coercion & `sret` Threshold | `va_list` IR Type & `va_arg` Lowering |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **x86_64 System V** | `sysv_x86_64` (1,067 LOC) | 6 (`RDI,RSI,RDX,RCX,R8,R9`) | 8 (`XMM0–XMM7`) | Recursive 2-eightbyte merge (`NO_CLASS < SSE < INTEGER < MEMORY`). `<= 16B` coerced to `{i64 / double / <2 x float>}`. `> 16B` (unless single `AVX`/`AVX512` vector): `byval(T)` param, `sret(T)` return | `[1 x {i32 gp_offset, i32 fp_offset, ptr overflow, ptr reg_save}]`; explicit CFG branching on `gp_offset <= 48 - 8*k` and `fp_offset <= 176 - 16*k` |
| **AArch64 AAPCS64 / Darwin / ARM64EC** | `aapcs64` (936 LOC) | 8 (`X0–X7`, +`X8` indirect result) | 8 (`V0–V7`) | 1–4 identical float/vector leaves -> **HFA/HVA** (`[N x float/double/vec]`). Non-HFA `<= 16B` -> `[1/2 x i64]`. `> 16B` -> caller temporary pointer (not `byval`), `sret(T)` in `X8` for return. ARM64EC builds hybrid x64/ARM64 thunk signatures | **Linux ELF**: `[1 x {ptr stack, ptr gr_top, ptr vr_top, i32 gr_offs, i32 vr_offs}]`. **Apple Darwin**: `ptr` (`char*`) with tighter sub-8B variadic packing |
| **Windows x64** | `win64` (553 LOC) | 4 positional (`RCX,RDX,R8,R9`) | 4 positional (`XMM0–XMM3`; 6 in `__vectorcall`) | Composites of **exact power-of-two size `{1, 2, 4, 8}` bytes** coerced to `i8/i16/i32/i64`. **All other sizes (`3, 5, 6, 7, >8` bytes)** passed by pointer to caller copy and returned via `sret(T)` in `RCX` (`RDX` for member functions!). `__vectorcall` passes 1–4 lane HVAs in `XMM0–XMM5` | `ptr` (`char*`); `va_arg` steps slot pointer by 8 bytes (dereferencing indirect pointer if size not in `{1,2,4,8}`) |
| **x86-32** | `x86_32` (294 LOC) | 0 (`cdecl`), 2 (`fastcall`: `ECX,EDX`), `ECX` (`thiscall`) | `XMM0–XMM2` (`vectorcall`) | Parameters on stack (`byval(T)` aligned to 4B). Return: `sret(T)` on Linux ELF for all aggregates (except Darwin/Win32 where `{1,2,4,8}`-byte POD structs return in `EAX`/`EDX:EAX`) | `ptr` (`char*`); 4-byte aligned pointer bump |
| **ARM32 AAPCS / VFP** | `arm32` (427 LOC) | 4 (`R0–R3`) | 16 `s0–s15` / 8 `d0–d7` (AAPCS-VFP) | AAPCS-VFP passes 1–4 lane float/double/NEON HFAs in VFP regs. Non-HFA composites `<= 4B` returned in `R0`, `> 4B` via `sret(T)`; params split into `[N x i32]` (`[N x i64]` for 8B-aligned) | `[1 x {ptr}]`; 4/8-byte aligned pointer bump |
| **RISC-V 64 (`lp64d`)** | `riscv64` (522 LOC) | 8 (`a0–a7`) | 8 (`fa0–fa7`) | Hardware FP struct flattening: 1-field `{fp}` -> `float/double`; 2-field `{fp,fp}`, `{fp,int}`, `{int,fp}` -> exact 2-element LLVM struct when FPRs/GPRs available! General aggregates `<= 16B` (`2*XLEN`) -> `[1/2 x i64]`; `> 16B` -> indirect pointer | `ptr` (`void*`); `XLEN` / `2*XLEN`-aligned pointer bump (indirect deref for `> 16B`) |
| **LoongArch64 (`lp64d`)** | `loongarch64` (517 LOC) | 8 (`$a0–$a7`) | 8 (`$fa0–$fa7`) | Mirrors RISC-V LP64D FP/GPR 1–2 field flattening (`fp`, `fp+fp`, `fp+gp`, `gp+fp`), `[1/2 x i64]` up to 16B, indirect reference `> 16B` | `ptr` (`void*`); 8/16-byte aligned slot traversal |
| **MIPS O32 / N64** | `mips` (536 LOC) | 4 (O32 `$a0–$a3`) / 8 (N64 `$a0–$a7`) | 2 (O32) / 8 (N64 `$f12–$f19`) | N64 passes `<= 16B` aggregates in 64-bit register chunks (`inreg` / `{i64, i64}`), `sret` for `> 16B` (O32 `sret` for all structs) | `ptr` (`void*`) |
| **PowerPC64 ELFv2** | `ppc64` (514 LOC) | 8 (`r3–r10`) | 13 (`f1–f13`) / 12 (`v2–v13`) | **Up to 8 homogeneous float/vector elements** passed/returned as HFA/HVA! Non-HFA `<= 16B` returned in `r3:r4` (`> 16B` `sret`); params up to 64B passed in `[N x i64]` GPR array | `ptr` (`char*`) in ELFv2 |
| **SystemZ (`s390x`)** | `s390x` (442 LOC) | 5 (`%r2–%r6`) | 4 (`%f0,%f2,%f4,%f6`) | Single-element `float`/`double` struct promoted to FPR! Other structs of size `{1,2,4,8}` bytes passed in single GPR; **all other sizes and all struct returns** use hidden indirect pointer / `sret` | `[1 x {i64 __gpr, i64 __fpr, ptr __overflow, ptr __reg_save}]` |
| **SPARC V8 / V9** | `sparc` (425 LOC) | 6 (`%o0–%o5`) | 16 (`%d0–%d30` in V9) | SPARC64 V9 passes/returns structs **up to 32 bytes** in `%o0–%o3` + FP regs; `> 32B` via indirect pointer / `sret` | `ptr` (`void*`) |
| **WebAssembly** | `wasm` (311 LOC) | Virtual SSA stack | Virtual SSA stack | Single-leaf scalar struct (ignoring empty fields/bases) unwrapped directly to its scalar type (`i32/i64/f32/f64/v128`); multi-field aggregates use `byval(T)` / `sret(T)` in linear memory | `ptr` (`void*`) in linear memory |

Both call-site marshalling (`llvm_gen_be_expr.cpp`) and callee prologue unpacking (`llvm_gen_be_decl.cpp`) execute symmetric inverse transformations: at the call site, a local `alloca` holding an aggregate is `memcpy`'d or loaded as the coerced register type (e.g., `{i64, double}`), passed in registers, and stored back into the parameter `alloca` in the callee's entry block.

### 5.6 Expression, Statement, and Target SIMD/System Intrinsics Lowering

#### Expressions (`src/llvm_gen_be_expr.cpp` — 4,697 LOC) & Intrinsics (`src/llvm_gen_be_intrinsics.cpp` — 557 LOC)
`llvm_lower_expr` and `llvm_lower_lvalue` implement every EDG `an_expr_node_kind` (`enk_operation`, `enk_constant`, `enk_variable`, `enk_temp_init`, `enk_dynamic_init`, `enk_new_delete`, `enk_throw`, `enk_typeid`, `enk_condition`, `enk_Reuse_value`, `enk_coroutine_await`, etc.):
- **Bitfield Lvalue Read-Modify-Write**: Loads the enclosing storage integer/byte array, extracts signed (`ashr`+`shl`) or unsigned (`lshr`+`and`) bit slices of width `field->bit_size` at `bit_offset`, and for stores masks out the target bit window before OR-ing the truncated RHS and storing back.
- **C11 / C++11 Atomics**: Maps `__atomic_load_n`, `__atomic_store_n`, `__atomic_exchange_n`, `__atomic_compare_exchange_n` (`CreateAtomicCmpXchg`), `__atomic_fetch_{add,sub,and,or,xor,nand}` (`CreateAtomicRMW`), and `__atomic_thread_fence` (`CreateFence`) across `Monotonic`, `Acquire`, `Release`, `AcquireRelease`, and `SequentiallyConsistent` orderings.
- **Padding Sanitization (`llvm_compute_type_padding_mask`)**: Recursively computes an exact per-byte bitmask of padding bytes and unallocated bitfield bits for `__builtin_clear_padding(ptr)` (required for C++20 `std::atomic<T>::compare_exchange_*` on structs with padding), zeroing only padding bits in memory while preserving live data fields.
- **Architecture SIMD & System Intrinsics (`llvm_gen_be_intrinsics.cpp`)**: Lowers x86 MMX/SSE/SSE2/AVX/AVX2/AVX-512 builtins (`__builtin_ia32_paddb128`, `vfmadd`, `blendv`, `movmsk`, `pause`, `rdtsc`, `clflush`), ARM64 NEON/SVE (`vaddq`, `vmulq`, `vld1q`, `vst1q`, `crc32`), RISC-V Vector (`vsetvli` / RVV intrinsics), and WebAssembly SIMD128 (`wasm_i32x4_make`, `splat`, `swizzle`).

#### Statements (`src/llvm_gen_be_stmt.cpp` — 1,469 LOC)
- **Iterative Statement-Chain Traversal**: EDG represents compound block bodies as singly-linked lists (`stmt->next`). In auto-generated parsers such as `sqlite3.c`'s `yy_reduce` (thousands of sequential statements in one function), naive recursion `llvm_lower_statement(stmt->next)` overflows the host thread stack. `llvm_lower_statement` iterates `for (a_statement_ptr cur = stmt; cur != nullptr; cur = cur->next)` in $O(1)$ stack space, recursing only on true lexical nesting (`if`, `while`, `switch`, `block`).
- **RAII Scope Unwind Emission (`emit_cleanups_to_depth`)**: Every `return`, `break`, `continue`, and `goto` compares the current `cleanup_stack.cleanups.size()` against the target's recorded cleanup depth (or forward-scans `goto` target scope depths), emitting destructor calls and VLA `llvm.stackrestore` calls in reverse LIFO order before branching.
- **Computed Gotos (`&&label` & `goto *expr`)**: Collects all address-taken labels in the function, lowers `&&label` via `llvm::BlockAddress::get(func, label_bb)`, and lowers `stmk_indirect_goto` into an `llvm::IndirectBrInst` populated with every address-taken destination block in the function.

### 5.7 Dual-ABI C++ Object Model: VTables, RTTI, and Pointers-to-Members

Unlike GCC (which speaks only the Itanium ABI on POSIX and PE-Itanium on MinGW), the LLVM backend achieves **native binary ABI compatibility with both `libstdc++`/`libc++` (Itanium C++ ABI) and Microsoft Visual C++ (`vcruntime` / `msvcrt` ABI)** selected automatically from `llvm::Triple::isKnownWindowsMSVCEnvironment()`:

#### 1. Virtual Tables & Thunks (`src/llvm_gen_be_vtable.cpp` — 1,159 LOC)
- **Itanium ABI (`_ZTV`, `_ZTT`, `_ZTC`)**:
  - Synthesizes complete vtable groups `[ offset_to_top (i64), &_ZTI<Class> (ptr), vfunc_0, vfunc_1, ... ]` (prefixed by `vbase_offset` and `vcall_offset` entries for virtual inheritance).
  - For virtual inheritance hierarchies, synthesizes the **Virtual Table Table (`_ZTT<Class>`)** and **Construction VTables (`_ZTC<Derived><Offset>_<Base>`)** passed to base subobject `C2`/`D2` constructors/destructors so virtual calls during base construction dispatch to the active base subobject rather than unconstructed derived overrides.
  - Synthesizes `this`-adjusting thunks (`_ZThn<nv>_...`, `_ZTv0_n<vcall>_...`) and covariant return-adjusting thunks (`_ZTch...` / `_ZTcv...`).
  - **Key Function Placement**: Evaluates Itanium §5.2.3 key-function rules — emitting `external` strong definitions when the class's first non-inline, non-pure virtual function is defined in the TU, and `linkonce_odr` COMDAT groups otherwise.
- **Microsoft ABI (`??_7` VFTable & `??_8` VBTable)**:
  - Separates virtual functions (**VFTable**, `??_7Class@@6B@`, preceded at slot `[-1]` by a pointer to `??_R4` `CompleteObjectLocator` when RTTI is enabled) from virtual base offsets (**VBTable**, `??_8Class@@7B@`, `i32` byte offsets from the `vbptr` field to each virtual base).

#### 2. Run-Time Type Information (`src/llvm_gen_be_rtti.cpp` — 738 LOC)
- **Itanium ABI**: Emits `_ZTS<Type>` (`linkonce_odr` null-terminated type name string) and `_ZTI<Type>` descriptors referencing the vtable of `_ZTVN10__cxxabiv117__class_type_infoE` (no bases), `__si_class_type_info` (single public non-virtual base), `__vmi_class_type_info` (multiple or virtual bases, encoding `__flags` = `non_diamond_repeat_mask (0x1) | diamond_shaped_mask (0x2)` and `(offset << 8) | virtual_mask (0x1) | public_mask (0x2)` per base), `__fundamental_type_info`, `__pointer_type_info`, and `__pointer_to_member_type_info`. Lowers `dynamic_cast<T>(p)` to `__dynamic_cast(sub_ptr, src_type_info, dst_type_info, src2dst_hint)` (and `dynamic_cast<void*>(p)` via `offset_to_top` load at `vptr[-2]`).
- **Microsoft ABI**: Synthesizes the complete 5-structure MSVC RTTI graph in `linkonce_odr` COMDATs (`??_R0` `TypeDescriptor`, `??_R1` `BaseClassDescriptor` with PMD `{mdisp, pdisp, vdisp}`, `??_R2` `BaseClassArray`, `??_R3` `ClassHierarchyDescriptor`, and `??_R4` `CompleteObjectLocator`, using 32-bit `IMAGE_REL_BASED_DIR32NB` image-relative offsets on x64/ARM64!) and lowers `dynamic_cast` to `__RTDynamicCast`.

#### 3. Pointers-to-Members (`src/llvm_gen_be_ptrmem.cpp` — 596 LOC)
- **Pointers-to-Data-Members**: `ptrdiff_t` byte offset; under Itanium, **null is represented by `-1` (`0xFFFFFFFFFFFFFFFF`)**, not `0` (because field offset `0` is a valid non-null member!), whereas MSVC uses `-1` (or `0` on virtual inheritance variations).
- **Pointers-to-Member-Functions**: `{ i64/ptr ptr, i64 adj }`:
  - **x86_64 Itanium**: `ptr` holds either the function address (even, LSB = 0) or `1 + vtable_byte_offset` (odd, LSB = 1); `adj` holds the `this` byte adjustment.
  - **ARM64 / AArch64 Itanium Divergence**: Because AArch64 instructions are 4-byte aligned and LSB of code pointers can interact with authentication/thumb conventions on ARM, AAPCS64 C++ ABI places the virtual discriminator bit in **`adj` (`adj = (this_adjustment << 1) | is_virtual`)**, while `ptr` holds the unshifted `vtable_byte_offset`! `llvm_gen_be_ptrmem.cpp` branches on `Triple::aarch64` to emit exact architecture-compliant virtual bit tests and `this` pointer shifts.

### 5.8 Zero-Cost Exception Handling: Itanium DWARF and Windows Funclet SEH

`src/llvm_gen_be_eh.cpp` (1,072 LOC) implements **two completely distinct LLVM IR exception architectures** depending on target OS ABI:

#### 1. Itanium Zero-Cost Unwinding (`__gxx_personality_v0`)
- Attaches `@__gxx_personality_v0` to any function containing `try` blocks, `noexcept` guards, or RAII destructors.
- Whenever `be_state->current_landing_pads` is non-empty (or `cleanup_stack` has active RAII objects), call expressions emit `llvm::InvokeInst` (`invoke ... to label %normal unwind label %lpad`) instead of `call`.
- Landing pads emit `landingpad { ptr, i32 } cleanup catch ptr @_ZTI...`:
  - Extracts `exc_ptr = extractvalue %lp, 0` and `sel = extractvalue %lp, 1`.
  - For each typed `a_handler_ptr` catch clause, calls `type_id = call i32 @llvm.eh.typeid.for.p0(ptr @_ZTI<CatchType>)`, compares `icmp eq i32 %sel, %type_id`, and branches to `%catch_body`.
  - Inside `%catch_body`: calls `adjusted_ptr = call ptr @__cxa_begin_catch(ptr %exc_ptr)`, binds/copies the catch parameter, pushes an EH cleanup frame calling `@__cxa_end_catch()` (so throwing inside a `catch` block still invokes `__cxa_end_catch`!), lowers the handler statement, calls `@__cxa_end_catch()`, and branches to `%try_cont`.
  - Unmatched exceptions execute active `ScopeCleanup` destructors and terminate with `resume { ptr, i32 } %lp` (or branch to an enclosing `try` block's landing pad).
- `enk_throw`: Allocates exception storage via `ptr @__cxa_allocate_exception(i64 sizeof(T))`, constructs/copies the thrown value into the buffer (routing any copy-constructor exception to `@__cxa_free_exception`), and calls `@__cxa_throw(ptr %buf, ptr @_ZTI<T>, ptr @dtor)` followed by `unreachable` (or `@__cxa_rethrow()` for empty `throw;`).

#### 2. Windows x64 / ARM64 Funclet-Based EH (`__CxxFrameHandler3` & `__C_specific_handler`)
LLVM's WinEH backend strictly forbids `landingpad` and `resume` when targeting Windows MSVC, requiring **outlined-ready token-threaded funclet pads**:
- **C++ `try`/`catch` (`__CxxFrameHandler3`)**: Emits `%cs = catchswitch within %parent_pad [label %catch1, label %catch2] unwind label %outer_cleanup` -> `%cp = catchpad within %cs [ptr @"??_R0?AUEx@@8", i32 flags, ptr %catch_var_alloca]` -> handler body -> `catchret from %cp to label %try_cont`.
- **RAII Destructors & SEH `__finally` (`cleanuppad`)**: Emits `%clpad = cleanuppad within %parent_pad []` -> destructor/finally calls -> `cleanupret from %clpad unwind label %next_pad`.
- **Mandatory Funclet Operand Bundles**: Every `call` or `invoke` emitted dynamically while `be_state->active_funclet_pads` is non-empty automatically receives `llvm::OperandBundleDef("funclet", active_funclet_pads.back())` — without which LLVM's `WinEHPrepare` pass aborts with `"Call inside funclet lacks funclet bundle"`!
- **Microsoft SEH `__try` / `__except(filter)` / `__finally` (`__C_specific_handler`)**: Outlines filter expressions and `__finally` blocks into helper functions accessing parent stack locals via `@llvm.localescape(...)` in the parent entry block and `@llvm.localrecover(ptr @parent_fn, ptr %fp, i32 idx)` in the filter/finally funclet! Supports `/EHa` asynchronous hardware exception tracking via `"seh-asynch"` function attributes (`llvm_enable_async_seh_attributes`).

### 5.9 C++20 Coroutines via Switch-Resumed `CoroSplit` Intrinsics

`src/llvm_gen_be_coro.cpp` (673 LOC) lowers EDG's `stmk_coroutine` descriptor (`a_coroutine_descr_ptr`) into LLVM's **Switch-Resumed Coroutine Representation**, delegating optimal frame packing, live-variable spilling across suspension points, and heap-allocation elision (HALO) to LLVM's mid-end `CoroEarly` -> `CoroSplit` -> `CoroElide` -> `CoroCleanup` passes:

```llvm
; Synthesized Coroutine Ramp & Pre-Split Structure (Conceptual IR)
define %Task @my_coro(i32 %x) presplitcoroutine {
entry:
  %promise = alloca %PromiseType, align 8
  %id = call token @llvm.coro.id(i32 16, ptr %promise, ptr null, ptr null)
  %need_alloc = call i1 @llvm.coro.alloc(token %id)
  br i1 %need_alloc, label %coro.alloc, label %coro.init

coro.alloc:
  %size = call i64 @llvm.coro.size.i64()
  %mem = call ptr @_Znwm(i64 %size)          ; or promise_type::operator new
  br label %coro.init

coro.init:
  %phi_mem = phi ptr [ null, %entry ], [ %mem, %coro.alloc ]
  %hdl = call ptr @llvm.coro.begin(token %id, ptr %phi_mem)
  ; Construct promise, copy parameters, evaluate get_return_object(), initial_suspend():
  ...
  ; At each co_await / co_yield (enk_coroutine_await):
  %save = call token @llvm.coro.save(ptr null)
  ; Symmetric transfer:await_suspend(handle) returns next std::coroutine_handle
  %susp = call i8 @llvm.coro.suspend(token %save, i1 false)
  switch i8 %susp, label %coro.suspend [
    i8 0, label %await.resume
    i8 1, label %await.cleanup
  ]
...
coro.final:
  %fsave = call token @llvm.coro.save(ptr null)
  %fsusp = call i8 @llvm.coro.suspend(token %fsave, i1 true) ; final = true
  ...
coro.cleanup:
  %mem_free = call ptr @llvm.coro.free(token %id, ptr %hdl)
  ; Conditional _ZdlPv(%mem_free) if non-null (skipped when HALO elided!)
coro.suspend:
  %unused = call i1 @llvm.coro.end(ptr null, i1 false, token none)
  ret %Task %gro
}
```
Coroutine handle intrinsics (`__builtin_coro_resume`, `__builtin_coro_destroy`, `__builtin_coro_done`, `__builtin_coro_promise`, `__builtin_coro_noop`) lower directly to `@llvm.coro.subfn.addr(ptr %hdl, i8 0/1)`, `@llvm.coro.done`, `@llvm.coro.promise`, and `@llvm.coro.noop`.

### 5.10 OpenMP 4.5 / 5.x Outlining and GPU Target Offloading

`src/llvm_gen_be_omp.cpp` (2,164 LOC) lowers OpenMP directives directly to Intel/LLVM `libomp` (`__kmpc_*`) and `libomptarget` (`__tgt_*`) ABI runtime calls:
1. **`#pragma omp parallel` Outlining**: Performs free-variable capture analysis over the structured block, packs shared variable pointers and `firstprivate` values into a `captured_struct`, synthesizes an internal microtask function `void .omp_outlined.(ptr noalias %.global_tid., ptr noalias %.bound_tid., ptr %.captured)`, emits an `ident_t` source-location descriptor (`{0, 2, 0, 0, ";file;func;line;col;;"}`), and emits `call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr @ident, i32 1, ptr @.omp_outlined., ptr %cap_alloca)`.
2. **Worksharing, Synchronization & Tasks**:
   - `#pragma omp for`: Static chunked/unchunked (`@__kmpc_for_static_init_4/8` + `@__kmpc_for_static_fini`) and dynamic/guided/runtime schedules (`@__kmpc_dispatch_init_4/8` + `@__kmpc_dispatch_next_4/8`), plus reduction tree accumulation (`+`, `-`, `*`, `&`, `|`, `^`, `&&`, `||`, `min`, `max`).
   - `#pragma omp sections`, `single` (`@__kmpc_single`/`end_single`), `master` (`@__kmpc_master`/`end_master`), `critical` (`@__kmpc_critical`/`end_critical` over 32-byte spinlock `kmp_critical_name`), `barrier` (`@__kmpc_barrier`), `task` (`@__kmpc_omp_task_alloc` + `@__kmpc_omp_task`), `taskwait` (`@__kmpc_omp_taskwait`), `flush` (`@__kmpc_flush` + `fence seq_cst`), `atomic`, and `#pragma omp simd` (`!llvm.loop.parallel_accesses` + `!"llvm.loop.vectorize.enable", i1 true`).
3. **`#pragma omp target [teams distribute parallel for]` GPU Offloading**:
   - Synthesizes `map(to:)`, `map(from:)`, `map(tofrom:)`, and `map(alloc:)` base/begin/size/type arrays (`OMP_TGT_MAPTYPE_TO = 0x1`, `FROM = 0x2`, `PARAM = 0x20`).
   - Emits `__tgt_kernel_arguments` struct calls to `@__tgt_target_kernel(ptr @ident, i64 -1, i32 num_teams, i32 thread_limit, ptr @.omp_offloading.entry, ptr %kernel_args)` with host fallback execution if `@__tgt_target_kernel` returns non-zero, and registers `.omp_offloading.entry` metadata in section `omp_offloading_entries` alongside `nvptx64-nvidia-cuda` / `amdgcn-amd-amdhsa` device kernel lowering.

### 5.11 Inline Assembly: GNU `callbr` and Intel/MSVC Dialect Translator

`src/llvm_gen_be_asm.cpp` (509 LOC) supports both major inline assembly ecosystems:
1. **GNU Extended `asm` and `asm goto`**:
   - Translates EDG's `%0`, `%w0`, `%x0`, `%%` template strings into LLVM's `$0`, `${0:w}`, `${0:x}`, `$$` syntax.
   - Canonicalizes constraints across x86/ARM64/RISC-V (`=r`, `+r` expanded into `=r` output + tied `"0"` input, early-clobber `=&r`, register names `{rax}`/`{x0}`, memory `=*m` / `*m`, plus implicit `~{dirflag},~{fpsr},~{flags}` on x86).
   - Multi-output `asm` instructions unpack LLVM's returned anonymous struct (`extractvalue`) into each destination lvalue, while `asm goto` emits `llvm::CallBrInst::Create` wired to normal fallthrough and indirect `BasicBlock*` label targets.
2. **MSVC `__asm { ... }` Block Translator**:
   - Tokenizes raw Intel-syntax assembly blocks, identifies stack-local C/C++ variable identifiers referenced inside instructions (`mov eax, my_local`), rewrites them to `$0`, `$1`, ... operands with synthesized `*m` / `=*m` constraints, infers modified x86/x64 registers (`eax`, `rcx`, `xmm0`, etc.) into explicit `~{reg}` clobbers, and constructs `llvm::InlineAsm::get(..., llvm::InlineAsm::AD_Intel)`.

### 5.12 Debug Information: DWARF v4/v5, Split DWARF, and CodeView

`src/llvm_gen_be_debug.cpp` (389 LOC), `llvm_gen_be_debug_scopes.cpp` (243 LOC), and `llvm_gen_be_debug_types.cpp` (955 LOC) emit production debug metadata through `llvm::DIBuilder`:
- **Format Selection**: Emits `"Dwarf Version"` (v4 or v5) and `"Debug Info Version"` (`DEBUG_METADATA_VERSION`) module flags on ELF/Mach-O, Split DWARF `.dwo` configuration via `TargetMachine::Options.MCOptions.SplitDwarfFile`, and `"CodeView" = 1` on Windows MSVC triples (enabling native `.pdb`-compatible `S_GPROC32_ID` / `LF_STRUCTURE` CodeView records!).
- **Cyclic Type Graph Resolution**: Uses `DIBuilder::createReplaceableCompositeType` cached in `dbg_state->type_map` before recursing into class fields, `DIDerivedType` `DW_TAG_inheritance` base records (including virtual base vtable offsets), member function prototypes, and template parameter packs (`DITemplateTypeParameter`, `DITemplateValueParameter`), replacing temporary nodes via `replaceTemporary`.
- **Template Instantiation Lexical Scope Isolation**: When EDG instantiates a C++17/20 template mid-function, lowering the instantiated routine must not nest its `DISubprogram` inside the caller's active `DILexicalBlock` stack. `llvm_gen_be_debug_scopes.cpp` snapshots and swaps the active `DIScope*` stack across out-of-line routine instantiations so every instruction's `DILocation::getScope()` chains strictly to its owning `DISubprogram`.

### 5.13 Optimization Pipeline (`PassBuilder`), Sanitizers, PGO, LTO, and Driver

- **`src/llvm_gen_be_opt.cpp` (252 LOC)**: Wires `LoopAnalysisManager`, `FunctionAnalysisManager`, `CGSCCAnalysisManager`, and `ModuleAnalysisManager` via `PassBuilder::crossRegisterProxies`.
  - At `-O0`, runs `AlwaysInlinerPass` + `CoroConditionalWrapper` (so C++20 coroutines still split at `-O0`!).
  - At `-O1`/`-O2`/`-O3`/`-Os`/`-Oz`, runs `buildPerModuleDefaultPipeline`, `buildThinLTOPreLinkDefaultPipeline` (`-flto=thin`), or `buildLTOPreLinkDefaultPipeline` (`-flto=full`).
  - Injects `AddressSanitizerPass`, `ModuleAddressSanitizerPass`, `ThreadSanitizerPass`, `MemorySanitizerPass`, `HWAddressSanitizerPass` (`-fsanitize=address,thread,memory,hwaddress`), function attributes (`sanitize_address`, `safestack`, `shadowcallstack`, `ssp`/`sspstrong`/`sspreq`), and PGO (`PGOOptions::IRInstr` / `IRUse` / `SampleUse`).
- **`src/llvm_gen_be_codegen.cpp` (372 LOC) & `llvm_gen_be_driver.cpp` (456 LOC)**: Thread-safely caches `TargetMachine` instances keyed by `(triple, cpu, features, reloc_model, code_model, opt_level)`, emits `.o`/`.s`/`.bc`/`.ll`, supports in-process `llvm::lto::LTO` linking, and invokes platform linkers (`clang++`/`gcc`/`ld`/`ld64`/`lld-link`/`link.exe`) with automatic sanitizer runtime libraries (`clang_rt.asan*`) and C++ standard library flags.

---

## 6. Internals of the GCC (`libgccjit`) Backend (`src/gcc_gen_be_*`)

### 6.1 Architectural Philosophy and the `libgccjit` Constraint Space

Targeting GCC's `libgccjit` C API presents a fundamentally different compiler-engineering problem from targeting LLVM's C++ SSA IR:

1. **Typed High-Level GIMPLE Builder vs. Low-Level SSA**: `libgccjit` exposes no explicit SSA values, PHI nodes, untyped `ptr` loads, or arbitrary `alloca` byte reinterpretation. Every expression is a strictly typed `gcc_jit_lvalue*` or `gcc_jit_rvalue*` validated eagerly by `libgccjit` upon construction; passing an `int*` where a `long*` is expected, or terminating a `gcc_jit_block*` twice (or zero times), immediately poisons the `gcc_jit_context*` and fails compilation.
2. **Opaque Struct Completeness Invariant**: Unlike LLVM (where an opaque `StructType` can be embedded by value inside pointer declarations and populated at any time), `libgccjit` forbids querying the size of or instantiating by-value fields containing an opaque `gcc_jit_struct*` whose `gcc_jit_struct_set_fields()` call has not yet completed.
3. **Absence of First-Class Upstream C++ Lowering Primitives**: Upstream `libgccjit` was originally designed for C-like JITs and dynamic language runtimes. Stock GCC 10–15 `libgccjit` distributions provide **no public C API** for C++ `try`/`catch` GIMPLE EH regions, landing pads, `invoke` abnormal edges, C++20 coroutine frame splitting, or GNU computed gotos (`goto *ptr`).
4. **In-Process GCC State & Licensing Boundary**: `libgccjit` executes GCC's `toplev::main` pass manager in-process inside a shared library (`libgccjit.so` / `libgccjit.dylib` / `libgccjit-0.dll`). Dynamically loading it at runtime avoids hard build-host coupling to a specific GCC major version.

The EDG GCC backend (`src/gcc_gen_be_*`, 32 files, 26,713 LOC + `lib_src/gcc_be_eh_rt.c`, 249 LOC) overcomes every one of these constraints while achieving full C23 and C++20/23 feature parity.

### 6.2 Zero-Dependency Dynamic Loader and Runtime Capability Probing (`gcc_gen_be_lib_loader.{h,c}`)

`src/gcc_gen_be_lib_loader.h` (567 LOC) and `src/gcc_gen_be_lib_loader.c` (606 LOC) decouple `cpfe` from compile-time `libgccjit` symbol availability through a dual-platform dynamic loader:

1. **Windows (`_WIN32`) Total Function-Pointer Redirection**:
   - `gcc_gen_be_lib_loader.h` defines function-pointer typedefs for all ~95 `libgccjit` C entrypoints (`pfn_gcc_jit_context_acquire`, ..., `pfn_gcc_jit_block_add_extended_asm`) and `#define`s each canonical `gcc_jit_*` identifier to `p_gcc_jit_*` across all GCC backend translation units.
   - At runtime, `gcc_gen_be_load_libgccjit()` probes `EDG_LIBGCCJIT_PATH`, `libgccjit-0.dll`, `libgccjit.dll`, and `C:\msys64\mingw64\bin\libgccjit-0.dll` via `LoadLibraryA` and resolves mandatory symbols via `GetProcAddress` (returning `GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED` or `GCC_GEN_BE_ERROR_LIBGCCJIT_SYMBOL_MISSING` if absent).
2. **POSIX (`dlopen` / `dlsym`) Version-Adaptive Capability Probing**:
   Because GCC 10, 11, 12, 13, 14, and 15 progressively added new `libgccjit` entrypoints, `gcc_gen_be_lib_loader.c` dynamically probes `RTLD_DEFAULT` (and `libgccjit.so.0` / Homebrew `libgccjit.0.dylib`) via `dlsym` to populate optional function pointers without breaking older GCC installations:

| Probed `libgccjit` Symbol | Minimum GCC | Fallback When Symbol Is Absent (`NULL`) |
| :--- | :---: | :--- |
| `gcc_jit_context_new_bitcast` | GCC 10+ | Stack-local temporary slot + pointer-cast dereference (`gcc_gen_be_coerce_rvalue`) |
| `gcc_jit_lvalue_set_tls_model` | GCC 10+ | Emits standard non-TLS or `__thread` asm fallback |
| `gcc_jit_lvalue_set_link_section` | GCC 10+ | Emits top-level `.section` / `.pushsection` assembly directives |
| `gcc_jit_global_set_initializer` / `_rvalue` | GCC 10/11+ | Synthesizes startup constructor assignment inside `__sti__gcc_be_init` |
| `gcc_jit_context_new_array_constructor` | GCC 11+ | Element-by-element runtime or global initializer fallback |
| `gcc_jit_context_new_struct_constructor` | GCC 11+ | Field-by-field temporary local initialization |
| `gcc_jit_function_get_return_type` / `get_param_count` | GCC 11+ | Queries cached EDG `a_routine_ptr` signature metadata |
| `gcc_jit_type_get_size` | GCC 12+ | Uses EDG front-end `tp->size` layout computation |
| `gcc_jit_lvalue_set_alignment` / `type_get_aligned` | GCC 12+ | Explicit padding byte-array synthesis |
| `gcc_jit_context_new_sizeof` | **GCC 14+** | Crucial GCC 13 fix: computes compile-time integer constant from EDG `tp->size`! |
| `gcc_jit_type_get_restrict` | GCC 14+ | Drops `restrict` qualifier cleanly while preserving pointer type |
| `p_gcc_jit_block_end_with_invoke`, `add_try_catch`, `add_cleanup`, `end_with_resx`, `function_set_personality` | Patched `libgccjit` | Automatically switches `gcc_gen_be_eh.c` to **Mode B (Hybrid SJLJ + Itanium RTTI Runtime)**! |

### 6.3 Context Stacks and the 8-Domain Open-Chaining Hash Cache (`gcc_gen_be_context.*`, `gcc_gen_be_cache.*`)

- **`gcc_gen_be_context_t` (`src/gcc_gen_be_context.h`)**: Maintains dynamic LIFO stacks for `break_stack`, `continue_stack`, `switch_exit_stack`, `block_stack`, `cleanup_stack` (RAII automatic variables and VLA stack saves), `landing_pad_stack`, `gcc_gen_be_seh_finally_entry_t` (`__try`/`__finally` abnormal-termination flag chains), `gcc_gen_be_coro_state_t` (reentrant nested coroutine frames), and `gcc_gen_be_eh_state_t` (per-function LSDA region/action/type-filter builders).
- **8-Domain Hash Cache (`src/gcc_gen_be_cache.c` — 306 LOC)**: Implements a custom C open-chaining hash map (`be_cache_map`, initial capacity `64`, load factor `0.75` geometric doubling, pointer shift-XOR hash `((k >> 3) ^ (k & 7u) ^ (k >> 48)) % cap` for AST pointers and DJB2 `5381` hash for strings) partitioned into 8 collision-free domains:
  1. `GCC_GEN_BE_CACHE_TYPE` (`a_type_ptr -> gcc_jit_type*`)
  2. `GCC_GEN_BE_CACHE_VAR` (`a_variable_ptr -> gcc_jit_lvalue*`)
  3. `GCC_GEN_BE_CACHE_FUNC` (`a_routine_ptr -> gcc_jit_function*`)
  4. `GCC_GEN_BE_CACHE_LABEL` (`a_label_ptr -> gcc_jit_block*`)
  5. `GCC_GEN_BE_CACHE_FIELD` (`a_field_ptr / a_base_class_ptr -> gcc_jit_field*`)
  6. `GCC_GEN_BE_CACHE_SWITCH_CASE` (case deduplication)
  7. `GCC_GEN_BE_CACHE_GUARD` (`a_variable_ptr -> _ZGV` static guard `gcc_jit_lvalue*`)
  8. `GCC_GEN_BE_CACHE_STRING` (interned string literal globals)

### 6.4 Topological Struct Cycle Breaking, Bitfields, Decimal FP (BID/DPD), and Fixed-Point (`gcc_gen_be_type.c`)

`src/gcc_gen_be_type.c` (1,242 LOC) introduces four notable type-lowering algorithms:

1. **Deferred Pointer-Edge Struct Population (`deferred_struct_entry_t[4096]`)**:
   Suppose `struct A { struct B *b; };` and `struct B { struct A a_by_val; };` are lowered starting at `A`. If lowering `B*` immediately populated `B`'s fields while `A` was still incomplete (`struct_field_lowering_depth > 0`), `B` would embed `A` by value before `gcc_jit_struct_set_fields(A)` had been called — triggering a fatal `libgccjit` error! `gcc_gen_be_type.c` tracks `struct_field_lowering_depth`: whenever a pointer targets an unpopulated struct while `struct_field_lowering_depth > 0`, it creates an **opaque** `gcc_jit_context_new_opaque_struct`, caches its `gcc_jit_type_get_pointer()`, queues a `deferred_struct_entry_t`, and drains the queue in topological order only after the outermost by-value `populate_struct_fields` finishes (`struct_field_lowering_depth == 0`).
2. **Exact Offset-Sorted Base + Bitfield Container Synthesis**:
   - Merges direct bases, virtual bases (deduplicated), and member fields into `layout_entry_t[]` sorted by `(byte_offset, bit_remainder)`.
   - Skips empty base subobjects (`is_optimized_empty_base`), truncates tail-padded base subobjects (`size_without_virtual_base_classes < bsize`) to `[bsize x char]` byte arrays so derived fields placed inside base tail padding never overlap in GIMPLE, and injects explicit `__pad_<offset>` arrays for alignment gaps.
   - Coalesces contiguous bitfield runs into alignment-safe unsigned integer storage containers (`__bf_container_<off>_<idx>` choosing `uint8_t`, `uint16_t`, `uint32_t`, or `uint64_t` bounded by the next non-bitfield member offset and natural alignment, plus `__bf_cont_ext_*` byte extensions for straddling bitfields).
3. **IEEE 754-2008 Decimal Floating-Point (`_Decimal32`, `_Decimal64`, `_Decimal128`)**:
   - Encodes decimal lexemes preserved by `src/lexical.c` directly into **Binary Integer Decimal (BID)** 32-bit (`uint32_t`), 64-bit (`uint64_t`), and 128-bit (`struct { uint64_t lo, hi; }`) bit patterns (`gcc_gen_be_encode_decimal32/64/128`), and implements bidirectional **Densely Packed Decimal (DPD) <-> BID** transcoding (`gcc_gen_be_dpd_to_bid{32,64,128}` / `gcc_gen_be_bid_to_dpd{32,64,128}` for PowerPC/s390x vs x86/AArch64 targets!).
   - Lowers arithmetic and comparisons into `libgcc` soft-DFP runtime calls (`__bid_addsd3`, `__bid_subdd3`, `__bid_multd3`, `__bid_divdd3`, `__bid_eqdd2`, etc.).
4. **ISO/IEC TR 18037 Embedded Fixed-Point (`_Fract`, `_Accum`, `_Sat`)**:
   Maps short/regular/long/unsigned `_Fract` and `_Accum` types to exact scaled two's-complement integer storage (`Q7`, `Q15`, `Q31`, `Q8.7`, `Q16.15`, `Q32.31`), synthesizing double-width fixed-point multiplication (`(a * b) >> fbits`), pre-shifted division (`(a << fbits) / b`), and branchless min/max clamping for saturating (`_Sat`) arithmetic (`gcc_gen_be_lower_fixed_point_binop`).

### 6.5 Multi-Target ABI Classification and Register Coercion (`gcc_gen_be_abi.{h,c}`)

`src/gcc_gen_be_abi.c` (1,775 LOC) implements **9 target ABI classifiers** (`SYSV_AMD64`, `AAPCS64`, `DARWIN_ARM64`, `WIN64`, `RISCV64_LP64D`, `SYSV_I386`, `PPC64LE_ELFV2`, `S390X`, `AAPCS32`) plus `gcc_gen_be_abi_reg_state_t` cumulative register allocation:
- **Invisible Reference & `sret` Transformation**: Non-trivially copyable C++ classes (and large aggregates exceeding target return limits: `>16B` on SysV/AAPCS64/RISC-V/PPC64LE, non-`{1,2,4,8}B` on Win64/s390x, all structs on Linux i386) are rewritten to pass an explicit `T*` parameter (allocated and copy-constructed at the call site) and prepend a hidden `T* __sret_ptr` parameter 0 returned by the callee.
- **Explicit Register Coercion (`gcc_gen_be_coerce_rvalue`)**: When a struct requires mixed register classes (e.g., `{int, float, double}` -> `{uint64_t, double}` on SysV AMD64, HFA/HVA on AAPCS64/PPC64LE, or `{fp, int}` on RISC-V LP64D), `gcc_gen_be_coerce_rvalue` coerces between the frontend struct type and the register-transfer struct using `p_gcc_jit_context_new_bitcast` when sizes match and `bitcast` is supported, or materializes a stack temporary local in the active `gcc_jit_block` and reinterprets its address as `dst_type*`.
- **Target-Specific `va_arg` CFG Synthesis**: Constructs explicit `gcc_jit_block` control-flow graphs updating `gp_offset`/`fp_offset`/`overflow_arg_area` on SysV AMD64, `gr_offs`/`vr_offs` on Linux AAPCS64, and aligned `char*` pointer bumps on Darwin ARM64, Win64, RISC-V, PPC64LE, and i386.

### 6.6 Computed Gotos, Inline Asm Constraint Splitting, and Explicit C++20 Coroutines (`gcc_gen_be_stmt.c`)

`src/gcc_gen_be_stmt.c` (3,515 LOC) solves three major GIMPLE lowering challenges without upstream `libgccjit` support:

#### 1. Computed Goto (`&&label` / `goto *ptr`) Per-Function Switch Dispatcher
`libgccjit` provides `gcc_jit_block_get_address()` in newer releases for static tables, but provides **no `gcc_jit_block_end_with_indirect_jump(block, rval)`** equivalent to LLVM's `indirectbr`!
`gcc_gen_be_stmt.c` solves this cleanly via **whole-function switch-dispatcher synthesis**:
- Taking the address of a label (`&&lbl`) registers `lbl` in `computed_goto_labels[]` and evaluates to `(void*)(uintptr_t)lbl` (a unique compile-time non-null token per label, supporting `&&l1 == &&l2`, pointer arrays `static void * table[] = { &&l1, &&l2 }`, and pointer arithmetic `&&l2 - &&l1`!).
- Every `goto *target_expr;` assigns `(long)(uintptr_t)target_expr` into a function-local variable `computed_goto_target_lval` and jumps to a lazily created `computed_goto_dispatcher_block`.
- At function finalization (`gcc_gen_be_finalize_computed_goto`), `computed_goto_dispatcher_block` is terminated with a single `gcc_jit_block_end_with_switch` mapping each `(long)(uintptr_t)lbl` case directly to its `gcc_jit_block*` (with `__builtin_trap()` on the unreachable default branch), which GCC's RTL pass compiles into a fast indirect jump table!

#### 2. GNU Extended `asm` Read-Write Constraint Normalization (`gcc_gen_be_normalize_asm_constraints`)
`gcc_jit_extended_asm_add_output_operand` rejects combined read-write `+r` / `+m` constraints in several GCC versions unless decomposed into pure output + tied input operands. `gcc_gen_be_normalize_asm_constraints` rewrites `+X` at output index `i` into output constraint `=X` plus a synthetic tied input constraint `"<i>"` referencing the same lvalue.

#### 3. Explicit AST-to-GIMPLE C++20 Coroutine Frame & State-Machine Synthesis
Because `libgccjit` has no equivalent to LLVM's `CoroSplit` pass, `gcc_gen_be_stmt.c` (lines 2740–3475) implements **complete front-end-driven coroutine transformation** directly into standard GIMPLE functions and structs:

```
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ Synthesized Coroutine Frame: struct _Z_coro_frame_<rout>                   │
  ├────────────────────┬───────────────────────────────────────────────────────┤
  │ Field 0            │ void (*__resume_fn)(void *frame)                      │
  │ Field 1            │ void (*__destroy_fn)(void *frame)                     │
  │ Field 2            │ PromiseType __promise                                 │
  │ Field 3            │ int __suspend_index       (0 = initial, 1..N = susp)  │
  │ Field 4            │ bool initial_await_resume_called                      │
  │ Field 5            │ int __coro_flags          (bit 0 = HALO stack-elided) │
  │ Fields 6..6+P-1    │ Copied function parameters (param_<var>)              │
  │ Fields 6+P..M      │ Hoisted cross-suspend local variables (loc_<var>)     │
  └────────────────────┴───────────────────────────────────────────────────────┘
```
*Note*: Placing `__resume_fn` at offset `0` and `__destroy_fn` at offset `sizeof(void*)` matches the **standard Itanium / Clang / GCC C++20 coroutine ABI layout**, allowing `std::coroutine_handle<P>::from_address(ptr).resume()` in standard `<coroutine>` headers (`__builtin_coro_resume` / `__builtin_coro_destroy`) to interoperate seamlessly!

The transformation emits three coordinated `gcc_jit_function`s:
1. **Ramp Function (`gcc_gen_be_lower_coroutine_ramp`)**:
   - Runs **HALO Escape Analysis (`gcc_gen_be_try_coro_halo`)**: If `-O2` or higher is active and the coroutine handle cannot escape the caller's lifetime, allocates `struct _Z_coro_frame` directly on the ramp stack (`coro_halo_stack_frame`) and sets `__coro_flags = 1`!
   - Otherwise calls custom `Promise::operator new(size, args...)`, over-aligned `_ZnwmSt11align_val_t(size, align)` when `exact_frame_align > 16`, or global `_Znwm(size)`. If `descr->alloc_failure_gro_call` is present (`noexcept` `operator new`), emits a null check returning `Promise::get_return_object_on_allocation_failure()`.
   - Initializes `__resume_fn = &_Z_coro_resume_<rout>`, `__destroy_fn = &_Z_coro_destroy_<rout>`, `__suspend_index = 0`, copies parameters into frame slots, constructs `__promise`, evaluates `get_return_object()`, evaluates `initial_suspend()`, and returns the return object.
2. **Resume Actor (`_Z_coro_resume_<rout>`)**:
   - Rebinds `GCC_GEN_BE_CACHE_VAR` entries for all coroutine parameters, `__promise`, and hoisted locals to `gcc_jit_lvalue_access_field(*coro_frame, field_i)` so ordinary expression/statement lowering transparently reads and writes the heap frame!
   - Wraps the body in an `unhandled_exception()` landing pad when required by the promise type.
   - At each `co_await` / `co_yield` suspension point $k \in \{1..N\}$: stores `__suspend_index = k`, evaluates `await_suspend(handle)` (supporting `void`, `bool` conditional resumption, and **C++20 symmetric transfer** where returning a `std::coroutine_handle<>` tail-dispatches `next_hdl.__resume_fn(next_hdl)`!), returns to the resumer, and registers `resume_block[k]` in `active_coro_state` so the entry `switch (__suspend_index)` jumps straight to `resume_block[k]` upon resumption!
3. **Destroy Actor (`_Z_coro_destroy_<rout>`)**:
   - Destroys live scope locals and `__promise`, checks `(__coro_flags & 1) == 0` (skipping heap deallocation if HALO stack-elided!), and invokes `Promise::operator delete` / sized or aligned `_ZdlPv`.

### 6.7 Dual-Mode Exception Handling, Unwind Table Encoders, and Hybrid Runtime (`gcc_gen_be_eh.c` + `lib_src/gcc_be_eh_rt.c`)

How can a compiler using unmodified upstream `libgccjit` pass complex C++ exception suites — including multiple/virtual diamond inheritance catches (`catch (VBase&)`), pointer qualification conversions (`throw Derived*` caught by `catch (const Base*)`), nested `try`/`catch` rethrows (`throw;`), RAII destructor unwinding, and interoperability with `std::runtime_error`?

`src/gcc_gen_be_eh.c` (2,669 LOC) and `lib_src/gcc_be_eh_rt.c` (249 LOC) implement a **Dual-Mode Adaptive Exception Architecture**:

```
                        ┌──────────────────────────────────────────────┐
                        │ gcc_gen_be_probe_native_eh_support()         │
                        │ dlsym("gcc_jit_block_end_with_invoke", ...)  │
                        └──────────────────────┬───────────────────────┘
                                               │
                    ┌──────────────────────────┴──────────────────────────┐
                    │ Patched GIMPLE EH Present                           │ Stock Upstream libgccjit (Default)
                    ▼                                                     ▼
   ┌─────────────────────────────────────────┐           ┌──────────────────────────────────────────────────┐
   │ MODE A: Native Zero-Cost GIMPLE EH      │           │ MODE B: Hybrid SJLJ + Itanium RTTI Runtime       │
   │ • p_gcc_jit_function_set_personality    │           │ • Thread-local stack: gcc_be_eh_frame_t          │
   │   (__gxx_personality_v0 / SEH)          │           │   { jmp_buf buf, void *exc_obj, void *tinfo,     │
   │ • p_gcc_jit_block_end_with_invoke       │           │     int selector, prev }                         │
   │ • p_gcc_jit_block_add_try_catch / resx  │           │ • try entry: __gcc_be_eh_push_frame(&frame)      │
   │ • Binary Unwind Table Encoders:         │           │              + setjmp(frame.buf) == 0            │
   │   - DWARF .gcc_except_table LSDA        │           │ • throw: __cxa_allocate_exception + __cxa_throw  │
   │     (ULEB128 call-sites + SLEB128 TType)│           │ • lib_src/gcc_be_eh_rt.c intercepts __cxa_throw: │
   │   - Win64 .pdata + .xdata UNWIND_INFO   │           │   - If top SJLJ frame active -> longjmp(buf, 1)  │
   │   - ARM32 .ARM.extab + .ARM.exidx       │           │   - Else -> dlsym(RTLD_NEXT, "__cxa_throw")      │
   └─────────────────────────────────────────┘           │     or _Unwind_RaiseException (native DWARF!)    │
                                                         │ • Catch dispatch: __gcc_be_eh_match_catch(ct,tt) │
                                                         │   walks real Itanium _ZTI (__si / __vmi / __pbase│
                                                         │   + vtable vbase_offset) & adjusts Derived*->Base│
                                                         └──────────────────────────────────────────────────┘
```

#### Deep Dive into Mode A Binary Unwind Table Encoders (`gcc_gen_be_eh.c`)
For patched `libgccjit` builds and standalone ELF/COFF assembly verification (`test_gcc_be_eh_dwarf.cpp`), `gcc_gen_be_eh.c` includes complete binary unwind table generators:
- **DWARF v4/v5 LSDA Encoder (`gcc_gen_be_emit_lsda_table`)**: Encodes `.section .gcc_except_table,"a",@progbits` headers (`@LPStart` encoding, `@TType` `DW_EH_PE_indirect | DW_EH_PE_pcrel | DW_EH_PE_sdata4` encoding, ULEB128 call-site table length, call-site `(start, len, lpad, action)` tuples, SLEB128 chained action records, and reverse-indexed `.long _ZTI...` type info tables).
- **Windows x64 SEH Tables (`gcc_gen_be_emit_seh_unwind_tables`)**: Emits `.pdata` (`RUNTIME_FUNCTION` `{BeginAddress, EndAddress, UnwindInfoAddress}` RVA triplets) and `.xdata` (`UNWIND_INFO` version 1 with `UNW_FLAG_EHANDLER | UNW_FLAG_UHANDLER` and `__gxx_personality_seh0` / `__C_specific_handler` RVA).
- **32-bit ARM EHABI Tables (`gcc_gen_be_emit_arm_ehabi_tables`)**: Emits `.ARM.exidx` (`R_ARM_PREL31` function entries) and `.ARM.extab` descriptors supporting both ARM compact model `PR0` (`0x80` single-word inline unwind opcodes) and generic `__gxx_personality_v0` LSDA descriptors.

#### Deep Dive into Mode B Hybrid Itanium RTTI Matcher (`lib_src/gcc_be_eh_rt.c`)
Why is `lib_src/gcc_be_eh_rt.c` far more capable than a naive `strcmp(type_name)` SJLJ runtime? Because `gcc_gen_be_rtti.c` emits **genuine Itanium C++ ABI `_ZTI` structures** linked against `libstdc++`/`libc++abi`'s `_ZTVN10__cxxabiv1...` vtables! When `__cxa_throw(thrown_object, tinfo, dest)` executes:
1. It stores `tinfo` and `dest` inside the Itanium `__cxa_exception` header **immediately preceding `thrown_object`** (`((gcc_be_cxa_exception_header_t *)thrown_object) - 1`), maintaining 100% binary layout compatibility with `libstdc++` / `libc++abi`.
2. If `tls_eh_top != NULL`, it pops `tls_eh_top`, records `exc_obj` and `tinfo`, and calls `longjmp(f->buf, 1)` into the generated `try` landing block. (If `tls_eh_top == NULL` — e.g., an EDG-compiled function called by `clang++`/`g++` code with no inner `try` — it delegates via `dlsym(RTLD_NEXT, "__cxa_throw")` or `_Unwind_RaiseException` so external DWARF frames catch it!).
3. In the landing block, generated code calls `__gcc_be_eh_match_catch(catch_zti, throw_zti, exc_obj)`. `gcc_be_match_abi_type_info()` inspects the vtable pointer `vptr` of `throw_zti` against `__si_class_type_info`, `__vmi_class_type_info`, and `__pbase_type_info`:
   - For **`__si_class_type_info`**: Recursively matches `__base_type`.
   - For **`__vmi_class_type_info`**: Iterates `__base_count` entries of `__base_info[i]`. It skips inaccessible private/protected bases (`!(flags & 0x2)`), computes `offset = flags >> 8` (arithmetic shift), and for **virtual bases (`flags & 0x1`)**, reads the thrown object's runtime `vptr` (`*(const ptrdiff_t *const *)cur_ptr`) at byte offset `offset` to dynamically adjust `Derived*` to `VBase*` across diamond hierarchies!
   - For **`__pbase_type_info` (pointers)**: Dereferences the thrown pointer (`void *pointee = *(void **)exc_obj`), verifies CV qualification (`(__flags & ~ct->__flags) == 0`), recursively upcasts `Derived*` -> `Base*` on `pointee`, and returns the adjusted pointer value!

### 6.8 Declarations, `__dso_handle`, Expressions, Array Cookies, and Builtins

1. **Declarations & COMDAT/Weak Assembly Synthesis (`src/gcc_gen_be_decl.c` — 1,249 LOC, `gcc_gen_be_dtor.c` — 112 LOC)**:
   - Lowers `GCC_JIT_GLOBAL_EXPORTED`, `GCC_JIT_GLOBAL_INTERNAL`, and `GCC_JIT_GLOBAL_IMPORTED` globals with TLS models (`GCC_JIT_TLS_MODEL_*`), section placement (`p_gcc_jit_lvalue_set_link_section`), and explicit alignment.
   - Where `libgccjit` lacks native COMDAT group objects for vague-linkage inline variables/vtables, `gcc_gen_be_apply_comdat_linkage` emits `.weak <sym>` (ELF), `.weak_definition <sym>` (Mach-O), or `.linkonce discard` (COFF/MinGW) via `gcc_jit_context_add_top_level_asm`.
   - Static destructors register with `@__cxa_atexit(dtor_fn, obj_ptr, __dso_handle)` via `gcc_gen_be_dtor.c`, ensuring shared-library (`dlclose`) unloading runs destructors in reverse construction order.
2. **Expressions & Constants (`src/gcc_gen_be_expr.c` — 5,305 LOC)**:
   - **128-Bit Integer Lowering**: Reconstructs multi-part EDG 128-bit constants via `gcc_jit_context_get_int_type(ctx, 16, is_signed)` (`__int128`) on 64-bit hosts, shifting and OR-ing two 64-bit halves because `libgccjit` provides no 128-bit literal constructor (`gcc_jit_context_new_rvalue_from_long` is capped at `long`).
   - **Itanium Array `new[]` / `delete[]` Cookie Synthesis**: For arrays of types with non-trivial destructors or two-argument `operator delete[](void*, size_t)`, allocates `max(sizeof(size_t), alignof(T))` header padding ahead of element 0, stores the element count $N$ at `((size_t*)data_ptr)[-1]`, constructs elements $0..N-1$ in a forward GIMPLE loop (cleaning up already-constructed elements if an element constructor throws!), and in `delete[]` reads the cookie to destroy elements $N-1..0$ in reverse order before freeing the adjusted base pointer.
   - **Pointers-to-Members**: Represents pointers-to-member-functions as `struct { fn_ptr_or_vtable_off __pfn; ptrdiff_t __delta; }`, implementing both x86_64 (`__pfn & 1`) and AArch64 (`__delta & 1`) virtual dispatch bit conventions.
   - **Vector Permutations & Complex Arithmetic**: Lowers `__builtin_shufflevector` / `__builtin_convertvector` via `p_gcc_jit_context_new_rvalue_from_vector` and lane-by-lane extraction, and decomposes `_Complex` multiplication/division (`__muldc3`/`__divdc3` or inline Smith's formula).
3. **Builtins & Atomics (`src/gcc_gen_be_builtin.c` — 1,171 LOC)**:
   - Maps `__atomic_*` and legacy `__sync_*` builtins directly to `gcc_jit_context_get_builtin_function("__atomic_load_4", ...)` across 1, 2, 4, 8, and 16-byte widths (verifying natural alignment via `GCC_GEN_BE_ERROR_ATOMIC_ALIGNMENT_FAILED`), checked arithmetic overflow (`__builtin_{s,u}{add,sub,mul}_overflow`), bit manipulation (`clz`, `ctz`, `popcount`, `bswap`, `ffs`, `clrsb`), math/FP classification (`isnan`, `isinf`, `isfinite`, `copysign`, `fma`), and `__builtin_clear_padding`.

### 6.9 Itanium C++ ABI Substitution Mangler, VTables, VTTs, Thunks, and RTTI (`gcc_gen_be_rtti.c`)

`src/gcc_gen_be_rtti.c` (2,128 LOC) implements a standalone Itanium C++ ABI vtable and RTTI engine:
- **Base-36 Substitution Compression (`gcc_gen_be_mangle_with_substitutions`)**: Tracks up to 64 composite scope/type prefixes in `gcc_gen_be_mangle_subst_table_t`, emitting `St` for `::std`, `S_` for substitution index 0, and `S<base36(i-1)>_` (`S0_`..`SZ_`, `S10_`..) for repeated namespace, class, and template-argument prefixes, plus `W<len><mod>` C++20 module purview qualifiers for non-exported module-attached entities (`source_corresp.module_info && !source_corresp.is_exported`).
- **Complete Polymorphic Tables (`_ZTV`, `_ZTT`, `_ZTC`)**: Synthesizes primary and secondary vtable groups (`offset_to_top`, `&_ZTI`, virtual function pointers, `vcall`/`vbase` offsets), construction vtables (`_ZTC`), and Virtual Table Tables (`_ZTT`) initialized via `p_gcc_jit_global_set_initializer_rvalue` (or startup constructor fallback on older `libgccjit`).
- **Must-Tail Adjusting Thunks**: Emits non-virtual (`_ZThn<off>_`) and virtual (`_ZTv0_n<vcall>_`) `this`-adjusting thunks and covariant return-adjusting thunks (`_ZTch` / `_ZTcv`), marking non-covariant thunks with `p_gcc_jit_rvalue_set_bool_require_tail_call(call, 1)` so variadic virtual functions forward arguments without stack-frame copying.

### 6.10 Debug Locations and Rich DWARF Metadata Synthesis (`gcc_gen_be_location.c`, `gcc_gen_be_debug.c`)

`libgccjit`'s built-in `GCC_JIT_BOOL_OPTION_DEBUGINFO` emits `.debug_line` source mappings from `gcc_jit_location*` (`gcc_gen_be_location.c`), plus C-level struct/function DIEs, but omits C++-specific DWARF constructs. `src/gcc_gen_be_debug.c` (529 LOC):
1. Configures `-g0`..`-g3`, `-gdwarf-2`..`-gdwarf-5`, `-fdebug-macro` (on ELF), `-fdebug-types-section`, and `-fno-eliminate-unused-debug-types` via `gcc_jit_context_add_command_line_option`.
2. Records and emits structured DWARF metadata descriptors for C++ namespaces (`DW_TAG_namespace`), class inheritance (`DW_TAG_inheritance` with `DW_AT_data_member_location` and `DW_AT_accessibility`), virtual member functions (`DW_AT_virtuality` and `DW_AT_vtable_elem_location` sourced from `rout->virtual_function_number` added to `il_def.h`), template type/value parameters (`DW_TAG_template_type_parameter`), scoped `enum class` enumerators (`DW_TAG_enumeration_type`), and nested lexical blocks (`DW_TAG_lexical_block`), attaching `GCC_JIT_FN_ATTRIBUTE_ARTIFICIAL` to compiler-synthesized thunks and ctors.

### 6.11 Main Orchestrator, `eccp.sh` Driver, and `lib_src/` Runtime Integration

- **`src/gcc_gen_be_main.c` (854 LOC)**: Maps EDG CLI globals (`gcc_be_opt_level`, `gcc_be_fPIC`, `gcc_be_march`, `gcc_be_dump_gimple`, etc.) onto `gcc_jit_context`, lowers all declarations/types/routines/vtables/module-initializers, invokes `gcc_jit_context_compile_to_file` (`GCC_JIT_OUTPUT_KIND_OBJECT_FILE`, `ASSEMBLER`, `DYNAMIC_LIBRARY`, or `EXECUTABLE`), and links multi-TU invocations together with `libgcc_be_eh_rt` when linking executables.
- **`lib_src/CMakeLists.txt` & `util/eccp.sh`**: `lib_src` compiles `gcc_be_eh_rt.c` directly into the EDG runtime archive (`libedg.a` / `edg_lib`), while `util/eccp.sh` (+37 lines) recognizes `--gcc` / `--gcc-be` / `--llvm`, bypasses legacy `.int.c` host C compilation and template prelinker (`edg_prelink`) loops when a native backend emits `.o` directly, and feeds generated native object files straight into the final link step (`nm | edg_decode` preserved when requested).

---

## 7. Head-to-Head Architectural Synthesis: LLVM vs. GCC vs. Legacy Backends

The coexistence of four backends inside a single front-end codebase offers a rare controlled experiment in compiler architecture. Below is a 24-dimension technical comparison across all four EDG backends:

| Dimension | `cp_gen_be` (`cpfe-cp`) | `c_gen_be` (`--c-be`) | `llvm_gen_be` (`--llvm`) | `gcc_gen_be` (`--gcc`) |
| :--- | :--- | :--- | :--- | :--- |
| **Implementation Size** | 2 files / 27,093 LOC | 2 files / 12,333 LOC | **63 files / 32,716 LOC** | **32 files / 26,713 LOC (+249 rt)** |
| **Implementation Language** | C-in-C++ | C-in-C++ | Modern C++17 (`noexcept`) | C-idiomatic C++ (`noexcept`) |
| **EDG `DO_IL_LOWERING`** | `0` (Unlowered AST/IL) | `1` (Full C lowering) | `1` (Partial: `PORTABLE_EH=0`) | `1` (Partial: `PORTABLE_EH=0`) |
| **Backend Target IR** | C++ Source Text (`.int.cpp`) | ANSI C Source Text (`.int.c`) | **LLVM SSA IR (In-Memory/BC/LL)** | **`libgccjit` Typed GIMPLE -> RTL** |
| **Host Integration Model** | None (Text I/O) | External C compiler subprocess | Static/Shared C++ link (LLVM 16–23+) | **`dlopen` / `LoadLibraryA` C API (GCC 10–15+)** |
| **Error Handling Discipline** | `internal_error` (`longjmp`) | `internal_error` (`longjmp`) | **`[[nodiscard]]` 40-code enum** | **`[[nodiscard]]` 82-code enum** |
| **Target psABIs Implemented** | Delegated to host C++ | Delegated to host C | **12 Hardware Architectures** | **9 Hardware Architectures** |
| **C++ Object Model ABIs** | Host C++ ABI | Lowered C structs + Itanium | **Both Itanium ABI & Windows MSVC ABI** | **Itanium C++ ABI** (`_ZTV`, `_ZTT`, `_ZTI`) |
| **Exception Unwinding** | Native (via host C++) | Portable SJLJ (`lower_eh.c`) | **Zero-Cost DWARF (`landingpad`) + Win64 SEH Funclets (`catchpad`)** | **Dual-Mode: Native GIMPLE/LSDA or Hybrid SJLJ + Itanium RTTI Bridge** |
| **Microsoft SEH (`__try`)** | Pass-through text | Unsupported / Partial | **`__C_specific_handler` + `localescape`** | **Explicit state-flag / SJLJ lowering** |
| **C++20 Coroutines** | Pass-through text | Lowered C switch frames | **LLVM `CoroSplit` (`llvm.coro.*`)** | **Explicit `_Z_coro_frame` + HALO** |
| **C++20 Modules (`export`)** | Source / BMI | Lowered C init | **Native `_ZGIW<mod>` idempotent init** | **Native `_ZGIW<mod>` + purview mangling** |
| **OpenMP 4.5 / 5.x** | Pragma pass-through | Ignored | **Full `__kmpc_*` + GPU `__tgt_*` offload** | Host builtin / pragma pass-through |
| **GNU Computed Goto (`&&l`)** | Pass-through text | Pass-through GNU C | **Native `blockaddress` + `indirectbr`** | **Synthesized per-function `switch` dispatcher** |
| **Inline Assembly** | Pass-through text | GNU C `asm` | **GNU `callbr` + MSVC `__asm` translator** | **GNU `gcc_jit_extended_asm` (`+r` split)** |
| **C23 `_BitInt(N)`** | Text | Emulated / Host C | **Native `llvm::IntegerType(N)`** | Widened integer / array representation |
| **IEEE 754 Decimal FP** | Text | Unsupported | Unsupported in upstream LLVM IR | **Full `_Decimal32/64/128` BID/DPD + `__bid_*`** |
| **ISO TR 18037 Fixed-Point** | Text | Emulated | Front-end / Scaled Int | **Full `_Fract` / `_Accum` / `_Sat` scaling** |
| **Alias Metadata** | None | C `restrict` | **Full C/C++ Scalar & Struct-Path TBAA** | `gcc_jit_type_get_restrict` + typed GIMPLE |
| **Debug Information** | `#line` directives | `#line` directives | **DWARF v4/v5, Split `.dwo`, MSVC CodeView** | **`gcc_jit_location` + custom `.debug_info` DIEs** |
| **Sanitizers** | Via downstream compiler | Partial (loses C++ types) | **ASan, TSan, MSan, HWASan, UBSan, SafeStack, SCS** | Via GCC driver options |
| **LTO & PGO** | Via downstream compiler | Via downstream C compiler | **Native ThinLTO, FullLTO (`llvm::lto`), IR/Sample PGO** | GCC `-flto` / context opt options |
| **Compilation Pipeline Hops** | 2 processes (EDG + C++) | 2 processes (EDG + C) | **1 process (In-memory IR -> `.o`)** | **1 process (In-memory GIMPLE -> `.o`)** |
| **License Boundary** | Apache-2.0 WITH LLVM-exception | Apache-2.0 WITH LLVM-exception | Apache-2.0 WITH LLVM-exception | Apache-2.0 source; runtime `dlopen` of GPLv3+ `libgccjit` |

### Key Theoretical Observation: *Where Should Semantic Lowering Live?*

Comparing `llvm_gen_be` and `gcc_gen_be` reveals a fundamental compiler design trade-off between **Intrinsic-Deferred Mid-End Lowering** (LLVM) and **Explicit Front-End/Bridge Lowering** (`libgccjit`):
- In **C++20 Coroutines**, LLVM's `llvm.coro.*` intrinsics let `llvm_gen_be_coro.cpp` (673 LOC) emit a single linear pre-split function **before** SROA and inlining, allowing `CoroSplit` to compute minimal live-variable frame layouts *after* mid-end dead-code and mem2reg optimizations. Conversely, because `libgccjit` lacks coroutine intrinsics, `gcc_gen_be_stmt.c` must construct `_Z_coro_frame_<rout>` and hoist scope variables *before* GIMPLE SSA optimization — requiring explicit HALO escape heuristics in the backend, yet achieving total independence from mid-end coroutine pass internals.
- Conversely, in **Target ABI Aggregate Classification**, **neither** LLVM IR nor `libgccjit` fully abstracts platform C/C++ psABIs (especially non-trivial C++ copy invisibility, SysV mixed `INTEGER`+`SSE` eightbytes, Windows non-power-of-two indirect slots, and RISC-V `{fp, int}` struct flattening). Consequently, *both* backends had to implement multi-thousand-line ABI engines (`6,544 LOC` in LLVM vs. `2,269 LOC` in GCC), confirming recent compiler literature (e.g., ClangIR / MLIR ABI dialect proposals) that C/C++ psABI lowering remains duplicatingly coupled to front-end type trees unless elevated to a shared typed IR layer.
- In **Decimal Floating-Point (`_Decimal32/64/128`)**, GCC's `libgcc` ecosystem (`__bid_*` / DPD) gave `gcc_gen_be` a clear advantage over LLVM IR, which still lacks first-class IEEE 754-2008 decimal floating-point IR types.

---

## 8. Verification Methodology, 100% Coverage Gates, and Empirical Corpus Study

### 8.1 Test Suite Architecture (`tests/` — 88 Backend Test Files, 37,352 LOC)

1. **LLVM Backend Suite (`tests/*llvm*` — 41 files, 25,326 LOC)**:
   - Direct C++ GoogleTest / standalone harnesses compiling synthetic and real C/C++20/23 programs in-process through EDG + `llvm_gen_be`, verifying emitted LLVM IR patterns, running `llvm::verifyModule`, JIT-executing or native-linking binaries, and checking exit codes/outputs across `test_llvm_expressions.cpp` (5,151 LOC), `test_llvm_declarations.cpp` (3,228 LOC), `test_llvm_abi.cpp` (2,555 LOC), `test_llvm_control_flow.cpp` (1,482 LOC), `test_llvm_vtable_rtti.cpp` (1,469 LOC), `test_llvm_const_evaluation.cpp` (1,290 LOC), `test_llvm_type_lowering.cpp` (1,211 LOC), `test_llvm_orchestrator.cpp` (1,142 LOC), `test_llvm_debug_info.cpp` (920 LOC), `test_llvm_builtins.cpp` (778 LOC), `test_llvm_exceptions.cpp` (726 LOC), `test_llvm_omp.cpp` (694 LOC), `test_llvm_coro_atomics.cpp` (653 LOC), `test_llvm_target_intrinsics.cpp` (375 LOC), `test_llvm_inline_asm.cpp` (346 LOC), `test_llvm_msvc_asm.cpp` (313 LOC), `test_llvm_error.cpp` (298 LOC), `test_llvm_lto.cpp`, `test_llvm_pgo.cpp`, `test_llvm_modules.cpp`, and `fuzz_llvm_backend.cpp` (`LLVMFuzzerTestOneInput` libFuzzer target).
2. **GCC Backend Suite (`tests/gcc_be/*` — 47 files, 12,026 LOC)**:
   - Combines **isolated unit tests** (`EDG_GCC_BE_ISOLATED_TEST`, mocking/constructing EDG AST nodes directly in memory to exercise internal error paths, bitfield packers, DWARF DIE encoders, LSDA/SEH/ARM-EHABI table writers, and all 9 target ABI classifiers even on non-native hosts) with **end-to-end execution tests** (`test_gcc_be_coroutines.cpp` 758 LOC, `test_gcc_be_bitfields.cpp` 744 LOC, `test_gcc_be_stmt2.cpp` 729 LOC, `test_gcc_be_expr.cpp` 699 LOC, `test_gcc_be_decl.cpp` 656 LOC, `test_gcc_be_abi_targets.cpp` 654 LOC, `test_gcc_be_eh_dwarf.cpp` 617 LOC, `gcc_be_eh_rt.cpp` 564 LOC, `test_gcc_be_rtti_full.cpp` 490 LOC, `test_gcc_be_simd.cpp` 387 LOC, etc.).

### 8.2 Automated 100% Documentation and Code Coverage Gates (`dev_tools/`)

| Verification Gate | Tool / Script | Enforced Invariant |
| :--- | :--- | :--- |
| **LLVM Doxygen Coverage** | `dev_tools/check_doc_coverage.py` + `Doxyfile.llvm_be` | **100% `@file`, `@brief`, `@param`, `@return` Doxygen coverage** across all `src/llvm_gen_be*.{h,cpp}` and `tests/test_llvm_*.cpp` |
| **GCC Doxygen XML Gate** | `dev_tools/check_gcc_be_doc_coverage.py` | Runs `doxygen` XML extraction (`EXTRACT_ALL=NO`, `WARN_IF_UNDOCUMENTED=YES`) over `src/gcc_gen_be_*` and asserts **zero undocumented functions, structs, enums, typedefs, macros, or fields** |
| **LLVM 4-Metric Code Coverage** | `dev_tools/check_llvm_be_coverage.py` | Compiles `src/llvm_gen_be*.cpp` with `-fprofile-instr-generate -fcoverage-mapping -O0 -g`, merges all `.profraw` profiles via `llvm-profdata`, and enforces **100.00% Function, 100.00% Line, 100.00% Region, and 100.00% Branch coverage** in `llvm-cov report` |
| **GCC Backend Coverage** | `dev_tools/check_gcc_be_coverage.sh` + `tests/gcc_be/check_coverage.py` | Builds `tests/gcc_be` with `--coverage` (`gcov`/`lcov`) and validates line/function/branch coverage across `src/gcc_gen_be_*.c` |
| **4-Mode Matrix Verifier** | `dev_tools/verify_backend_modes.py` | Builds all 4 `(LLVM={0,1} × GCC={0,1})` CMake configurations and verifies runtime CLI behavior and IR/object generation |

### 8.3 Empirical Case Study: Real-World Bug Taxonomy Exposed by `sqlite3.c` (255k LOC) and Cross-OS CI

Synthetic unit tests alone did not catch several subtle systems bugs that only manifested when compiling industrial codebases — most notably the single-file **SQLite3 amalgamation (`sqlite3.c`, >255,000 lines of dense C11)** — and executing the 6-target OS/CPU CI matrix (Linux x86_64/ARM64, macOS x86_64/ARM64, Windows MSVC x64/ARM64). Seven bugs discovered and fixed during `multibackend` stabilization merit particular attention for compiler builders:

1. **Preprocessor `sizeof()` Silently Zeroing Upper 64 Bits of Multi-Part Negative Constants (`cad5d4fa64`)**:
   - *Symptom*: Negative `int64_t`/`__int128_t` bitmasks in `sqlite3.c` and constant-evaluation tests had their upper bits silently zeroed when `INTEGER_VALUE_REPR_IS_A_HOST_INTEGER == 0`.
   - *Root Cause*: An `#if` guard in `llvm_gen_be_internal.h` wrote `#if INT128_EXTENSIONS_ALLOWED && sizeof(an_integer_value) >= 16`. In C/C++ preprocessor evaluation (`[cpp.cond]`), `sizeof` is an unknown identifier replaced with `0`, producing `0(0) >= 16` (a syntax error or false branch depending on macro expansion), causing `EDG_INT_VAL_HIGH64` to fall into the 64-bit sign-extension stub instead of reading `val.part[0..3]`!
   - *Fix*: Replaced preprocessor `sizeof` with `#if INT128_EXTENSIONS_ALLOWED || FIXED_POINT_ALLOWED` (the exact macros controlling `INT_VALUE_PARTS_PER_INTEGER_VALUE == 8` in EDG's `const_ints.h`), and used runtime/compile-time C++ `(sizeof(val) > 8)` inside expression macros.
2. **Host Thread Stack Overflow on 10,000-Statement Generated Functions (`26dac043d3`)**:
   - *Symptom*: Segfault inside `cpfe` when lowering `sqlite3.c`'s Lemon-generated LALR(1) parser (`yy_reduce`).
   - *Root Cause*: Tail-recursing on `stmt->next` consumed $O(N)$ C++ stack frames proportional to sequential statement count in a single block.
   - *Fix*: Converted both `llvm_lower_statement` and `gcc_gen_be_lower_statement` into iterative `while (stmt)` loops with $O(D)$ stack depth bounded strictly by lexical block nesting depth $D$.
3. **C-Mode `is_trivially_copyable()` Misclassifying All C Structs as Non-Trivial (`26dac043d3`)**:
   - *Symptom*: ABI mismatch when `sqlite3.c` (compiled by `cpfe`) passed small structs by value to libc / external C functions or function pointers.
   - *Root Cause*: `src/types.c:2254` checked `if (symbol_for(tp) == NULL) result = FALSE;`. In C mode, EDG does not attach C++ `a_class_symbol_supplement` records to C `struct` types, causing both backends' ABI engines to treat every C `struct` as a non-trivially copyable C++ class passed by invisible pointer!
   - *Fix*: Updated `src/types.c` to return `TRUE` in `C_mode()` (unless virtual functions/bases exist).
4. **Tentative Incomplete Global Array Definitions in C (`26dac043d3`)**:
   - *Symptom*: `sqlite3.c` forward-declares `static const unsigned char sqlite3OpcodeProperty[];`, references `sqlite3OpcodeProperty[op]` inside functions, and defines `static const unsigned char sqlite3OpcodeProperty[] = { ... };` 50,000 lines later.
   - *Fix*: Implemented RAUW (`replaceAllUsesWith`) global type upgrading in `llvm_gen_be_decl.cpp` and deferred tentative global emission in `gcc_gen_be_decl.c`.
5. **GCC 13 `libgccjit` Missing `gcc_jit_context_new_sizeof` (`f29f38f2f5`)**:
   - *Symptom*: Dynamic symbol lookup failure on Ubuntu 24.04 (`libgccjit-13-dev`), because `gcc_jit_context_new_sizeof` was introduced in GCC 14.
   - *Fix*: Reclassified `gcc_jit_context_new_sizeof` as an optional probed symbol in `gcc_gen_be_lib_loader.c` and computed `sizeof(T)` directly from EDG's target layout `tp->size`.
6. **Linux ARM64 `HOST_ALIGNMENT_REQUIRED` Macro-Conf Mismatch (`d4ff3c0231`)**:
   - *Symptom*: Misaligned IL arena allocation faults on `linux-arm64` builds.
   - *Fix*: Aligned `HOST_ALIGNMENT_REQUIRED` in `cmake/macro-conf/linux-arm-*` to `8` bytes matching 64-bit AArch64 pointer alignment.
7. **Windows ARM64 SoftFloat Symbol Collisions & `f128M_to_f64` (`27055b4bc9`, `f29f38f2f5`)**:
   - *Symptom*: MSVC ARM64 `<arm_neon.h>` typedefs `float32_t` and `float64_t` as fundamental FP types, colliding with Berkeley SoftFloat's `typedef struct { uint32_t v; } float32_t;`.
   - *Fix*: Scoped `#define float32_t edg_softfloat32_t` around `#include "softfloat.h"` in `src/targ_def.h` and updated `src/float_pt.c`.

---

## 9. Research Takeaways and Open Compiler Engineering Problems

1. **Decoupling Semantic C++ Lowering from Portable C Emission**:
   EDG's historical `lower_il` pass conflated *C++ semantic desugaring* (overload resolution, constructor/destructor call synthesis, base subobject layout) with *C source transpilation* (`setjmp`/`longjmp` EH tables). Splitting that boundary via a single boolean (`DO_FULL_PORTABLE_EH_LOWERING = FALSE`) allowed two modern native backends (~59k LOC combined) to reuse ~75k LOC of battle-tested C++23 front-end lowering while emitting zero-cost native DWARF/SEH unwinding and SSA/GIMPLE coroutines.
2. **Standardizing Upstream `libgccjit` C++ & Unwind Extensions**:
   `src/gcc_gen_be_eh.c` demonstrates the exact minimal 5-function C API extension (`gcc_jit_function_set_personality`, `gcc_jit_block_end_with_invoke`, `gcc_jit_block_add_try_catch`, `gcc_jit_block_add_cleanup`, `gcc_jit_block_end_with_resx`) plus `gcc_jit_block_end_with_indirect_jump` needed in upstream GCC so ahead-of-time C++/Rust/Ada front ends can emit native zero-cost DWARF/SEH GIMPLE EH without requiring SJLJ runtime bridges (`lib_src/gcc_be_eh_rt.c`) or whole-function computed-goto switch synthesis.
3. **Three-Way Differential Translation Validation (`llvm_gen_be` vs. `gcc_gen_be` vs. `c_gen_be`)**:
   Because `cpfe` parses a C++23 translation unit **once** into a deterministic EDG IL graph and can lower that identical IL graph through LLVM SSA (`--backend=llvm`), GCC GIMPLE (`--backend=gcc`), or ISO C (`--backend=c`), `edgcpp-compiler-multibackend` provides a uniquely clean **single-front-end, multi-optimizer differential testing oracle** for isolating mid-end/back-end miscompilations in LLVM and GCC from front-end parsing ambiguities.

---

## Appendix A: Complete Multi-Backend CLI Option Reference (`src/cmd_line.{h,c}`)

| CLI Option(s) | Option Enum (`an_option_kind`) | Backend | Description |
| :--- | :--- | :---: | :--- |
| `--backend={llvm\|gcc\|c\|cpp}` | `optk_backend_select` | All | Unified runtime backend selector |
| `--llvm` | `optk_llvm_gen_be` | LLVM | Explicitly selects the LLVM IR backend |
| `--gcc`, `--gcc-be` | `optk_gcc_gen_be` | GCC | Explicitly selects the GCC (`libgccjit`) backend |
| `--c-be` | `optk_c_gen_be` | C | Explicitly selects the legacy C-generating backend |
| `--emit-llvm[=<file>]` | `optk_gen_llvm_file_name` | LLVM | Emits human-readable LLVM IR (`.ll`) |
| `--emit-bc[=<file>]` | `optk_gen_llvm_bc_file_name` | LLVM | Emits binary LLVM bitcode (`.bc`) |
| `--emit-thinlto-index` | `optk_gen_llvm_thinlto_index` | LLVM | Emits ThinLTO `.thinlto.bc` module summary index |
| `--emit-obj=<file>` / `-c` | `optk_gen_obj_file_name` / `optk_gcc_be_c` | LLVM / GCC | Emits native relocatable object file (`.o` / `.obj`) |
| `--emit-asm=<file>` / `-S` | `optk_gen_asm_file_name` / `optk_gcc_be_S` | LLVM / GCC | Emits native target assembly (`.s`) |
| `-o <file>` | `optk_output_file` / `ofile_name` | LLVM / GCC | Sets output object, assembly, IR, shared lib, or linked executable path |
| `-O0`, `-O1`, `-O2`, `-O3`, `-Os`, `-Oz`, `-Ofast` | `optk_llvm_opt_level` / `optk_gcc_be_O*` | LLVM / GCC | Configures LLVM `PassBuilder` or `GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL` |
| `-g` / `--llvm-debug-info` | `optk_llvm_debug_info` / `optk_gcc_be_g` | LLVM / GCC | Emits DWARF v4/v5 or Windows CodeView debug metadata |
| `--gsplit-dwarf[=<file>]` | `optk_llvm_split_dwarf` | LLVM | Emits Split DWARF `.dwo` debug object |
| `--target=<triple>` | `optk_llvm_target` | LLVM | Sets cross-compilation target triple (12 architectures + NVPTX/AMDGPU) |
| `--mcpu=<cpu>`, `--mattr=<+feat>` | `optk_llvm_mcpu`, `optk_llvm_mattr` | LLVM | Configures target CPU microarchitecture and feature flags |
| `-march=`, `-mcpu=`, `-mtune=`, `-mfpu=`, `-mcmodel=`, `-m64`, `-m32` | `optk_gcc_be_m*` | GCC | Passes target architecture/ABI flags to `gcc_jit_context_add_command_line_option` |
| `--reloc-model={static\|pic\|ropi\|rwpi}` | `optk_llvm_reloc_model` | LLVM | Sets `llvm::Reloc::Model` |
| `-fPIC`, `-fPIE`, `-fno-pie`, `-shared`, `-static` | `optk_gcc_be_f*` / `shared` / `static` | GCC | Configures PIC/PIE codegen and shared/static output modes |
| `--code-model={tiny\|small\|kernel\|medium\|large}` | `optk_llvm_code_model` | LLVM | Sets `llvm::CodeModel::Model` |
| `-flto[={full\|thin}]` | `optk_flto` | LLVM | Enables FullLTO or ThinLTO pre-link pipeline and embedded LLD/LTO linking |
| `-fprofile-generate=<path>`, `-fprofile-use=<path>` | `optk_fprofile_{generate,use}` | LLVM | Enables IR/Sample Profile-Guided Optimization (`PGOOptions`) |
| `-fsanitize={address,thread,memory,undefined,hwaddress,safestack,shadow-call-stack}` | `optk_fsanitize` | LLVM | Injects LLVM sanitizer instrumentation passes and linker runtimes |
| `-fstack-protector[-strong\|-all]` | `optk_fstack_protector` | LLVM | Sets function stack-canary protection level (`ssp`, `sspstrong`, `sspreq`) |
| `-fopenmp` | `optk_llvm_fopenmp` | LLVM | Enables OpenMP 4.5/5.x directive parsing, `__kmpc_*` outlining, and GPU target offload |
| `-ffast-math`, `-fno-trapping-math`, `-fassociative-math` | `optk_gcc_be_f*math` | GCC | Configures `libgccjit` fast-math GIMPLE/RTL flags |
| `--gcc-dump-tree`, `--gcc-dump-gimple`, `--gcc-dump-rtl` | `optk_gcc_be_dump_*` | GCC | Enables `GCC_JIT_BOOL_OPTION_DUMP_INITIAL_{TREE,GIMPLE}` and RTL pass dumps |
| `-Wl,<arg>`, `-L<dir>`, `-l<lib>` | `optk_llvm_linker_arg`, `optk_gcc_be_{L,l}` | LLVM / GCC | Forwards library search paths, libraries, and raw flags to the linker driver |

---

## Appendix B: Complete `gcc_gen_be_error_t` Enumerator Reference (`src/gcc_gen_be_error.h`)

All **82** status codes in `enum GCC_GEN_BE_NODISCARD gcc_gen_be_error` (`0..81`) with exact integer ordinals:

| Code | Enumerator (`GCC_GEN_BE_...`) | Code | Enumerator (`GCC_GEN_BE_...`) |
| ---: | :--- | ---: | :--- |
| 0 | `SUCCESS` | 41 | `ERROR_COROUTINE_RAMP_FAILED` |
| 1 | `ERROR_OOM` | 42 | `ERROR_COROUTINE_DISPATCH_FAILED` |
| 2 | `ERROR_UNSUPPORTED` | 43 | `ERROR_CFI_UNWIND_FAILED` |
| 3 | `ERROR_LIBGCCJIT_LOAD_FAILED` | 44 | `ERROR_SIMD_LOWERING_FAILED` |
| 4 | `ERROR_LIBGCCJIT_SYMBOL_MISSING` | 45 | `ERROR_TARGET_OPTION_INVALID` |
| 5 | `ERROR_INVALID_ARGUMENT` | 46 | `ERROR_VA_ARG_LOWERING_FAILED` |
| 6 | `ERROR_INVALID_STATE` | 47 | `ERROR_MANGLING_BUFFER_OVERFLOW` |
| 7 | `ERROR_INTERNAL` | 48 | `ERROR_MANGLING_SUBSTITUTION_FAILED` |
| 8 | `ERROR_UNHANDLED_TYPE` | 49 | `ERROR_COOKIE_ALLOCATION_FAILED` |
| 9 | `ERROR_UNHANDLED_EXPR` | 50 | `ERROR_WIDE_INT_CONVERSION_FAILED` |
| 10 | `ERROR_UNHANDLED_STMT` | 51 | `ERROR_VLA_ALLOCATION_FAILED` |
| 11 | `ERROR_UNHANDLED_DECL` | 52 | `ERROR_LABEL_RESOLUTION_FAILED` |
| 12 | `ERROR_NULL_POINTER` | 53 | `ERROR_VECTOR_PERMUTE_FAILED` |
| 13 | `ERROR_TYPE_MISMATCH` | 54 | `ERROR_FLOAT_PRECISION_FAILED` |
| 14 | `ERROR_EH_FAILURE` | 55 | `ERROR_MULTI_TU_PIPELINE_FAILED` |
| 15 | `ERROR_EH_UNSUPPORTED` | 56 | `ERROR_DSO_HANDLE_FAILED` |
| 16 | `ERROR_EH_LANDING_PAD_FAILED` | 57 | `ERROR_COERCION_FAILED` |
| 17 | `ERROR_EH_TABLE_GENERATION_FAILED` | 58 | `ERROR_WEAK_LINKAGE_FAILED` |
| 18 | `ERROR_UNRESOLVED_SYMBOL` | 59 | `ERROR_UNWIND_CLEANUP_FAILED` |
| 19 | `ERROR_BUILTIN_UNKNOWN` | 60 | `ERROR_COROUTINE_PROMISE_FAILED` |
| 20 | `ERROR_BUILTIN_SIGNATURE_MISMATCH` | 61 | `ERROR_COROUTINE_SUSPEND_FAILED` |
| 21 | `ERROR_ABI_MISMATCH` | 62 | `ERROR_ATOMIC_ALIGNMENT_FAILED` |
| 22 | `ERROR_UNSUPPORTED_ATTRIBUTE` | 63 | `ERROR_EH_CFG_EDGE_FAILED` |
| 23 | `ERROR_VTABLE_GENERATION_FAILED` | 64 | `ERROR_COROUTINE_SPLIT_FAILED` |
| 24 | `ERROR_VTABLE_LAYOUT_FAILED` | 65 | `ERROR_ABI_PROLOGUE_UNPACK_FAILED` |
| 25 | `ERROR_RTTI_INVALID` | 66 | `ERROR_COMDAT_GROUP_FAILED` |
| 26 | `ERROR_RTTI_GENERATION_FAILED` | 67 | `ERROR_DWARF_SCOPE_FAILED` |
| 27 | `ERROR_RTTI_EMISSION_FAILED` | 68 | `ERROR_DWARF_DIE_FAILED` |
| 28 | `ERROR_ASM_CONSTRAINT_INVALID` | 69 | `ERROR_LSDA_ENCODE_FAILED` |
| 29 | `ERROR_DWARF_EMISSION_FAILED` | 70 | `ERROR_MODULE_IMPORT_FAILED` |
| 30 | `ERROR_DWARF_DIE_CREATION_FAILED` | 71 | `ERROR_MODULE_EXPORT_FAILED` |
| 31 | `ERROR_COMPILATION_FAILED` | 72 | `ERROR_CORO_TRANSFER_FAILED` |
| 32 | `ERROR_VTT_GENERATION_FAILED` | 73 | `ERROR_CORO_HALO_FAILED` |
| 33 | `ERROR_THUNK_GENERATION_FAILED` | 74 | `ERROR_FIXED_POINT_OVERFLOW` |
| 34 | `ERROR_ABI_CLASSIFICATION_FAILED` | 75 | `ERROR_DECIMAL_FLOAT_UNSUPPORTED` |
| 35 | `ERROR_UNSUPPORTED_CALLING_CONV` | 76 | `ERROR_DPD_FLOAT_CONVERSION_FAILED` |
| 36 | `ERROR_DWARF_TYPE_EMISSION_FAILED` | 77 | `ERROR_VLA_STACK_RESTORE_FAILED` |
| 37 | `ERROR_ASM_OPERAND_UNSUPPORTED` | 78 | `ERROR_DWARF_DIE_BINDING_FAILED` |
| 38 | `ERROR_STRUCT_PACKING_FAILED` | 79 | `ERROR_RELOCATION_EMISSION_FAILED` |
| 39 | `ERROR_BITFIELD_LAYOUT_FAILED` | 80 | `ERROR_TAIL_CALL_THUNK_FAILED` |
| 40 | `ERROR_COROUTINE_FRAME_FAILED` | 81 | `ERROR_TEST_ASSERTION_FAILED` |

---

## Appendix C: Branch Lineage, Multi-OS Release Matrix, and Reproducibility Guide

### C.1 Git Branch Topology (`main` -> `{llvm_backend, gcc_backend}` -> `multibackend`)

```
  main (upstream EDG open-source front end, ad4cfc4018)
   ├──► llvm_backend (12 commits: c3770edf17..7fa91abfbf)
   │      Initial SSA lowering -> 12 ABIs -> Itanium/MSVC RTTI & EH -> DWARF/CodeView -> noexcept + 100% coverage
   ├──► gcc_backend  (10 commits: cc0d088356..516331dafa)
   │      libgccjit loader -> modular 16-subsystem split -> Dual-Mode EH & SJLJ/ABI runtime -> C++20 coroutines -> noexcept
   └──► multibackend (35 total commits above main, HEAD 2f5df72de8 / tag v7.0.1)
          • Unified CMake 4-mode build matrix & runtime --backend={llvm,gcc,c,cpp} CLI
          • Upstream main merge reconciliation (4e68b31999)
          • sqlite3.c 255k-LOC amalgamation & cross-platform ABI/SoftFloat/LLVM 16–23+/GCC 13–15 remediation
          • 6-target OS/CPU CI & portable release packaging (.github/workflows/release.yml)
```

### C.2 Multi-OS / Multi-Architecture CI & Release Packaging (`.github/workflows/release.yml`)

The `multibackend` release pipeline builds, verifies (`verify_backend_modes.py` + `ctest`), bundles runtime libraries (`libedg`, `gcc_be_eh_rt`, standard headers `include_c++/`), generates `SHA256SUMS.txt`, and publishes self-contained archives across **6 native host targets**:
1. `linux-x86_64` (Ubuntu 22.04/24.04 + `linux_libc_compat.c` for glibc >= 2.17 / musl portability)
2. `linux-arm64` (Ubuntu ARM64 + AArch64 LLVM & `libgccjit`)
3. `macos-arm64` (Apple Silicon M-series, Homebrew LLVM + GCC `libgccjit.0.dylib`)
4. `macos-x86_64` (Intel macOS)
5. `windows-x64` (MSVC x64 + LLVM SDK + header-only/dynamic `libgccjit-0.dll`)
6. `windows-arm64` (MSVC ARM64 + `edg_softfloat32_t`/`64_t` neon-safe SoftFloat)

### C.3 Reproducible Verification Commands

```bash
# 1. Configure and build unified Dual-Backend compiler (Mode D: LLVM + GCC) + all 88 backend test suites
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DEDG_ENABLE_LLVM_BACKEND=ON \
  -DEDG_ENABLE_GCC_BACKEND=ON \
  -DBUILD_TESTING=ON
cmake --build build -j

# 2. Execute full LLVM + GCC backend CTest suite
ctest --test-dir build --output-on-failure -j

# 3. Verify all 4 CMake backend build combinations (Modes A, B, C, D)
python3 dev_tools/verify_backend_modes.py --all-modes --source-dir . --work-dir .tmp/mode-matrix

# 4. Verify 100% Doxygen documentation coverage across LLVM & GCC backends
python3 dev_tools/check_doc_coverage.py
python3 dev_tools/check_gcc_be_doc_coverage.py

# 5. Verify 100.00% Function, Line, Region, and Branch coverage on the LLVM backend
python3 dev_tools/check_llvm_be_coverage.py

# 6. Run PGO and Sanitizer end-to-end integration checks
bash run_pgo_test.sh
bash run_sanitizer_test.sh
```


