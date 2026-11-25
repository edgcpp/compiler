/**
 * @defgroup llvm_gen_be_omp OpenMP & GPU Offloading
 * @ingroup llvm_gen_be
 * @brief OpenMP 4.5/5.x host and GPU target offloading subsystem.
 */

/**
 * @file llvm_gen_be_omp.h
 * @ingroup llvm_gen_be_omp
 * @brief OpenMP lowering subsystem for the LLVM backend.
 * @details Declares functions for lowering OpenMP 4.5 / 5.x directives into `libomp` and
 *          `libomptarget` runtime calls.
 */

#ifndef LLVM_GEN_BE_OMP_H
#define LLVM_GEN_BE_OMP_H 1

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include <llvm/IR/Function.h>

BEGIN_EDG_NAMESPACE

/**
 * @enum omp_capture_attr_t
 * @brief OpenMP data-sharing attribute classification for captured variables.
 * @details Identifies whether a variable referenced inside an OpenMP region is shared,
 *          private, firstprivate, lastprivate, reduction, linear, or copyin.
 */
enum class omp_capture_attr_t : int {
  shared = 0,       ///< Variable is shared across threads (`shared`).
  priv = 1,         ///< Variable has uninitialized thread-private storage (`private`).
  firstprivate = 2, ///< Variable has copy-initialized thread-private storage (`firstprivate`).
  lastprivate = 3,  ///< Variable writes back its value on the last iteration (`lastprivate`).
  reduction = 4,    ///< Variable participates in a reduction clause (`reduction`).
  linear = 5,       ///< Variable advances linearly with the loop iteration (`linear`).
  copyin = 6        ///< Threadprivate variable copied from primary thread (`copyin`).
};

/**
 * @struct omp_capture_context_t
 * @brief Collects variables captured by an OpenMP parallel or task region and their data-sharing attributes.
 * @details Used by `lower_omp_parallel_region` to construct microtask parameter lists and private/firstprivate allocas.
 */
struct omp_capture_context_t {
  std::vector<a_variable_ptr> vars;           ///< Captured EDG variable pointers in microtask argument order.
  std::vector<llvm::Value*> parent_ptrs;      ///< Corresponding parent-function alloca/global pointers.
  std::vector<omp_capture_attr_t> share_attrs;///< Data-sharing attribute for each captured variable.
};

/**
 * @struct omp_map_clause_entry_t
 * @brief Represents a parsed OpenMP `map(...)` clause item and its `libomptarget` mapping flags.
 * @details Captures the mapped base expression or variable name, optional struct member or array
 *          section bounds (`arr[lower:length]`), byte size, and `OMP_TGT_MAPTYPE_*` bitmask.
 */
struct omp_map_clause_entry_t {
  std::string expr_text;        ///< Raw variable, member (`s.field`), or array-section (`a[0:N]`) token.
  std::string base_var_name;    ///< Base variable identifier before `.` or `[`.
  uint64_t map_type_flags;      ///< `OMP_TGT_MAPTYPE_*` bitmask (`TO=0x1`, `FROM=0x2`, `ALWAYS=0x4`, `DELETE=0x8`, `PTR_AND_OBJ=0x10`, `TARGET_PARAM=0x20`, `MEMBER_OF=0x1000000000000`).
  uint64_t section_offset_elems;///< Constant lower bound element offset for array sections (`0` if scalar).
  uint64_t section_length_elems;///< Constant element count for array sections (`1` if scalar).
  bool is_array_section;        ///< `true` if the clause specifies an `[lower:length]` array section.
  bool is_struct_member;        ///< `true` if the clause maps a struct/class member (`s.field`).
};

/**
 * @brief Parses all `map(...)`, `to(...)`, and `from(...)` clauses from an OpenMP directive string.
 * @details Recognizes `to`, `from`, `tofrom`, `alloc`, `release`, `delete`, and `always` modifiers,
 *          array section syntax (`arr[lower:length]`), and struct member access (`s.field`),
 *          populating `OMP_TGT_MAPTYPE_*` flags on each output entry.
 * @param[in] directive_text Null-terminated OpenMP directive text string.
 * @param[out] out_entries Receives the parsed list of `omp_map_clause_entry_t` descriptors.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if pointers are null.
 */
llvm_gen_be_error_t parse_omp_map_clauses(
    const char* directive_text,
    std::vector<omp_map_clause_entry_t>* out_entries) noexcept;

/**
 * @brief Lowers an OpenMP `target data` scoped data mapping construct into LLVM IR.
 * @details Emits `__tgt_target_data_begin_mapper` before lowering the structured body statement
 *          and `__tgt_target_data_end_mapper` after the body completes.
 * @param[in] stmt The EDG statement representing the `target data` body.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_data(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `target enter data` standalone directive into LLVM IR.
 * @details Populates mapper arrays (`base_ptrs`, `ptrs`, `sizes`, `map_types`) from parsed
 *          `map(to:/alloc:)` clauses and emits `__tgt_target_data_begin_mapper`.
 * @param[in] stmt The EDG statement associated with `omp target enter data`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_enter_data(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `target exit data` standalone directive into LLVM IR.
 * @details Populates mapper arrays from parsed `map(from:/release:/delete:)` clauses and
 *          emits `__tgt_target_data_end_mapper`.
 * @param[in] stmt The EDG statement associated with `omp target exit data`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_exit_data(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `target update` standalone directive into LLVM IR.
 * @details Populates mapper arrays from `to(...)` / `from(...)` clauses and emits
 *          `__tgt_target_data_update_mapper`.
 * @param[in] stmt The EDG statement associated with `omp target update`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_update(a_statement_ptr stmt) noexcept;

/**
 * @brief Compiles outlined OpenMP target kernels and `declare target` functions into a device image buffer.
 * @details Extracts `__omp_offloading_*` kernel functions and `omp_declare_target` routines from
 *          @p host_mod into a device `llvm::Module` for @p offload_triple and @p offload_arch,
 *          applies kernel calling conventions and attributes (`nvvm.annotations` or
 *          `amdgpu-flat-work-group-size`), and serializes the device image into @p out_device_image.
 * @param[in] host_mod The host LLVM Module containing outlined target kernels.
 * @param[in] offload_triple Target device triple string (e.g. `"nvptx64-nvidia-cuda"`, `"amdgcn-amd-amdhsa"`).
 * @param[in] offload_arch Target GPU architecture string (e.g. `"sm_80"`, `"gfx90a"`).
 * @param[out] out_device_image Receives the compiled device bitcode/object image bytes.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if pointers are null.
 */
llvm_gen_be_error_t compile_omp_device_module(
    llvm::Module* host_mod,
    const std::string& offload_triple,
    const std::string& offload_arch,
    llvm::SmallVectorImpl<char>* out_device_image) noexcept;

/**
 * @brief Lowers an OpenMP parallel region into LLVM IR.
 * @details Outlines the parallel body into `.omp_microtask.`, captures active local
 *          variables (`shared`/`private`/`firstprivate`), and emits `__kmpc_fork_call`.
 * @param[in] stmt The EDG statement representing the parallel region.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_parallel_region(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP for worksharing construct into LLVM IR.
 * @details Allocates loop bound stack slots (`plower`, `pupper`, `pstride`, `plastiter`),
 *          extracts canonical loop bounds and induction variable when present, and invokes
 *          static/dynamic OpenMP runtime dispatch.
 * @param[in] stmt The EDG statement representing the worksharing construct.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_for_worksharing(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP critical section into LLVM IR.
 * @details Emits a `[8 x i32]` global critical lock (`gomp_critical_user_.var`), calls
 *          `__kmpc_critical`, lowers the critical region body statement, and calls `__kmpc_end_critical`.
 * @param[in] stmt The EDG statement representing the critical section.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_critical(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP barrier synchronization into LLVM IR.
 * @details Emits a call to `__kmpc_barrier` with the directive's `ident_t` descriptor.
 * @param[in] stmt The EDG statement representing the barrier.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_barrier(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP target region for device offloading into LLVM IR.
 * @details Outlines the device kernel entry routine, allocates mapper arrays (`base_ptrs`,
 *          `ptrs`, `sizes`, `map_types`), and emits `__tgt_target_data_begin_mapper`,
 *          `__tgt_target_kernel`, and `__tgt_target_data_end_mapper`.
 * @param[in] stmt The EDG statement representing the target region.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_region(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP explicit task construct into LLVM IR.
 * @details Outlines the task body into `.omp_task_entry.`, allocates task descriptor
 *          storage via `__kmpc_omp_task_alloc`, and schedules it via `__kmpc_omp_task`.
 * @param[in] stmt The EDG statement representing the task body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_task(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `taskwait` synchronization directive into LLVM IR.
 * @details Emits a call to `__kmpc_omp_taskwait` and declares `__kmpc_omp_wait_deps`.
 * @param[in] stmt The EDG statement representing the taskwait directive.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_taskwait(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `taskgroup` construct into LLVM IR.
 * @details Wraps the body statement between `__kmpc_taskgroup` and `__kmpc_end_taskgroup`.
 * @param[in] stmt The EDG statement representing the taskgroup body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_taskgroup(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP reduction clause into LLVM IR.
 * @details Emits `__kmpc_reduce_nowait` and `__kmpc_end_reduce_nowait` with a reduction lock.
 * @param[in] stmt The EDG statement representing the reduction body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_reduce(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `sections` worksharing construct into LLVM IR.
 * @details Uses static worksharing initialization (`__kmpc_for_static_init_4` / `__kmpc_for_static_fini`)
 *          over the section range.
 * @param[in] stmt The EDG statement representing the sections construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_sections(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `section` construct into LLVM IR.
 * @details Delegates directly to `llvm_lower_statement` for the section body.
 * @param[in] stmt The EDG statement representing the section body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_section(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `single` construct into LLVM IR.
 * @details Guards execution of the body with `__kmpc_single` and `__kmpc_end_single`.
 * @param[in] stmt The EDG statement representing the single construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_single(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `taskyield` directive into LLVM IR.
 * @details Emits a call to `__kmpc_omp_taskyield`.
 * @param[in] stmt The EDG statement representing the taskyield directive.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_taskyield(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `flush` directive into LLVM IR.
 * @details Emits `__kmpc_flush` and a sequentially consistent memory fence.
 * @param[in] stmt The EDG statement representing the flush directive.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_flush(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `ordered` construct into LLVM IR.
 * @details Wraps the body statement between `__kmpc_ordered` and `__kmpc_end_ordered`.
 * @param[in] stmt The EDG statement representing the ordered body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_ordered(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `simd` loop construct into LLVM IR.
 * @details Lowers the loop statement and attaches `!llvm.loop` vectorization metadata
 *          (`!llvm.loop.vectorize.enable`, `!llvm.loop.vectorize.width`, `!llvm.loop.parallel_accesses`).
 * @param[in] stmt The EDG statement representing the simd loop.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_simd(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `teams` / `teams distribute` construct into LLVM IR.
 * @details Emits `__kmpc_push_num_teams` and `__kmpc_fork_teams` with an outlined team microtask.
 * @param[in] stmt The EDG statement representing the teams construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_teams_distribute(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `taskloop` or `taskloop simd` construct into LLVM IR.
 * @details Emits `__kmpc_omp_task_alloc` and `__kmpc_taskloop` to partition loop iterations across tasks.
 * @param[in] stmt The EDG statement representing the taskloop construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_taskloop(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers an OpenMP `master` or `masked` construct into LLVM IR.
 * @details Guards execution of the structured block with `__kmpc_master` (or `__kmpc_masked`)
 *          and `__kmpc_end_master` (or `__kmpc_end_masked`).
 * @param[in] stmt The EDG statement representing the master/masked construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_master(a_statement_ptr stmt) noexcept;

/**
 * @brief Dispatches an OpenMP pragma-annotated statement to the appropriate runtime lowering routine.
 * @details Inspects `be_state->omp_pragmas` and `stmt->attributes` when `stmt->has_associated_pragma`
 *          is set, invokes the matching `lower_omp_*` routine, and reports whether it was handled.
 * @param[in] stmt The EDG statement with an associated pragma.
 * @param[out] out_handled Receives `true` if an OpenMP construct was matched and lowered.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_dispatch_omp_pragma_stmt(
    a_statement_ptr stmt,
    bool* out_handled) noexcept;

/**
 * @brief Generates a device-triple offload bitcode buffer and embeds it into the host module.
 * @details Clones outlined `__omp_offloading_*` kernel functions into a secondary `llvm::Module`
 *          configured with `device_triple` (e.g., `nvptx64-nvidia-cuda` or `amdgcn-amd-amdhsa`),
 *          applies `PTX_Kernel` / `AMDGPU_KERNEL` calling conventions and `nvvm.annotations`,
 *          serializes the device module to LLVM bitcode, and embeds it into `.llvm.offloading`.
 * @param[in,out] host_module The host LLVM Module containing outlined target kernels.
 * @param[in] device_triple The target device triple string (e.g. `"nvptx64-nvidia-cuda"`).
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if arguments are null.
 */
llvm_gen_be_error_t llvm_emit_omp_device_offload_module(
    llvm::Module* host_module,
    const char* device_triple) noexcept;

/**
 * @brief Attaches OpenMP `declare simd` Vector Function ABI attributes to an LLVM function.
 * @details Emits `_ZGVbN4v_<name>` and `_ZGVdN8v_<name>` vector ABI variant attributes on @p func
 *          so the LLVM LoopVectorize pass can vectorize calls inside `#pragma omp simd` loops.
 * @param[in,out] func Pointer to the target `llvm::Function`.
 * @param[in] simdlen Optional explicit vector lane count (`0` to use default 4/8 lanes).
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p func is null.
 */
llvm_gen_be_error_t lower_omp_declare_simd(
    llvm::Function* func,
    uint32_t simdlen) noexcept;

/**
 * @brief Synthesizes an OpenMP `declare reduction` combiner and initializer function pair.
 * @details Creates `.omp.user_reduction.combiner.<reduction_id>` and
 *          `.omp.user_reduction.init.<reduction_id>` in the active LLVM module so user-defined
 *          `#pragma omp declare reduction` clauses can be passed to `__kmpc_reduce_nowait`.
 * @param[in] reduction_id Identifier string of the user-defined reduction (e.g., `"my_max"`).
 * @param[in] elem_ty LLVM type of the reduction variable.
 * @param[out] out_combiner_fn Receives the synthesized combiner `llvm::Function*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null.
 */
llvm_gen_be_error_t lower_omp_declare_reduction(
    const char* reduction_id,
    llvm::Type* elem_ty,
    llvm::Function** out_combiner_fn) noexcept;

/**
 * @brief Lowers an OpenMP `atomic` (`read`, `write`, `update`, `capture`, `compare`) construct into LLVM IR.
 * @details Emits monotonic/release/acquire atomic load, store, `atomicrmw`, or `cmpxchg` operations
 *          in the current basic block and lowers the associated statement body.
 * @param[in] stmt The EDG statement representing the OpenMP atomic construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_atomic(a_statement_ptr stmt) noexcept;

END_EDG_NAMESPACE

#endif
