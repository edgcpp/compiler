/**
 * @file llvm_gen_be_omp.cpp
 * @ingroup llvm_gen_be_omp
 * @brief OpenMP lowering subsystem for the LLVM backend.
 * @details Implements OpenMP 4.5 / 5.x runtime lowering (`libomp` / `libomptarget`)
 *          for parallel regions, worksharing loops, critical sections, barriers,
 *          device target regions, tasks, taskwait/taskgroup, reductions, and sections/single.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_omp.h"
#include "llvm_gen_be_internal.h"
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
#include <cctype>
#include <cstdlib>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Creates or retrieves an OpenMP `ident_t` source location descriptor global.
 * @details Binds `ident_t` per `LLVMContext` via `StructType::getTypeByName` and encodes
 *          the source position coordinates into the location string.
 * @param[in] M Pointer to the active LLVM Module.
 * @param[in] pos The EDG source position of the OpenMP directive.
 * @param[out] out_ident Receives the generated `ident_t` global variable constant.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if M or out_ident is null.
 */
static llvm_gen_be_error_t get_or_create_ident_t(
    llvm::Module* M,
    a_source_position pos,
    llvm::Constant** out_ident) noexcept {
    llvm::LLVMContext& Ctx = M->getContext();
    llvm::StructType* ident_ty = llvm::StructType::getTypeByName(Ctx, "ident_t");
    if (!ident_ty) {
        ident_ty = llvm::StructType::create(Ctx, {
            llvm::Type::getInt32Ty(Ctx),
            llvm::Type::getInt32Ty(Ctx),
            llvm::Type::getInt32Ty(Ctx),
            llvm::Type::getInt32Ty(Ctx),
            llvm::PointerType::getUnqual(Ctx)
        }, "ident_t");
    }

    std::string func_name = "unknown";
    llvm::StringRef fn_ref = be_state->builder->GetInsertBlock()->getParent()->getName();
    if (!fn_ref.empty()) {
        func_name = fn_ref.str();
    }
    std::string file_name = !M->getSourceFileName().empty() ? M->getSourceFileName() : "unknown";
    uint32_t line_no = static_cast<uint32_t>(pos.seq);
    uint32_t col_no = static_cast<uint32_t>(pos.column);
    std::string loc_str = ";" + file_name + ";" + func_name + ";" +
                          std::to_string(line_no) + ";" + std::to_string(col_no) + ";;";

    llvm::Constant* loc_const = llvm::ConstantDataArray::getString(Ctx, loc_str, true);
    llvm::GlobalVariable* loc_str_gv = nullptr;
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(M, loc_const->getType(), true, llvm::GlobalValue::PrivateLinkage, loc_const, ".omp.str", &loc_str_gv));

    llvm::Constant* ident_init = llvm::ConstantStruct::get(ident_ty, {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 2),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0),
        loc_str_gv
    });

    llvm::GlobalVariable* gv = nullptr;
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(M, ident_ty, true, llvm::GlobalValue::PrivateLinkage, ident_init, ".omp.ident", &gv));
    *out_ident = gv;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a call to `__kmpc_global_thread_num` to obtain the current OpenMP thread ID.
 * @details Declares `i32 @__kmpc_global_thread_num(ptr)` in the module if not already present
 *          and emits a call passing `ident`.
 * @param[in] M Pointer to the active LLVM Module.
 * @param[in] ident Pointer to the `ident_t` source location descriptor constant.
 * @param[out] out_gtid Receives the resulting `i32` global thread ID `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if arguments are null.
 */
static llvm_gen_be_error_t get_omp_global_thread_num(
    llvm::Module* M,
    llvm::Constant* ident,
    llvm::Value** out_gtid) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    llvm::LLVMContext& Ctx = M->getContext();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    llvm::FunctionType* gtid_ty = llvm::FunctionType::get(i32_ty, {ptr_ty}, false);
    llvm::FunctionCallee gtid_fn = M->getOrInsertFunction("__kmpc_global_thread_num", gtid_ty);
    *out_gtid = be_state->builder->CreateCall(gtid_fn, {ident}, "omp.global_tid");
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Retrieves the OpenMP directive text associated with a statement, if any.
 * @details Checks `be_state->omp_pragmas` and `stmt->attributes` token strings.
 * @param[in] stmt Pointer to the EDG statement node.
 * @param[out] out_text Receives the null-terminated directive string, or `nullptr` if none.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if null.
 */
static llvm_gen_be_error_t get_omp_directive_text(
    a_statement_ptr stmt,
    const char** out_text) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    *out_text = nullptr;
    auto it = be_state->omp_pragmas.find(stmt);
    if (it != be_state->omp_pragmas.end()) {
        *out_text = it->second;
        return llvm_gen_be_error_t::ok;
    }
    for (a_pragma_ptr p = be_state->active_pragma_list; p != nullptr; p = p->next) {
        if (p->entity.ptr == reinterpret_cast<char*>(stmt) && p->pragma_text) {
            *out_text = p->pragma_text;
            return llvm_gen_be_error_t::ok;
        }
    }
    for (an_attribute_ptr attr = stmt->attributes; attr != nullptr; attr = attr->next) {
        if (attr->arguments && attr->arguments->kind == aak_token && attr->arguments->variant.token) {
            *out_text = attr->arguments->variant.token;
            return llvm_gen_be_error_t::ok;
        }
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Unwraps `tk_typeref` sugar nodes without requiring front-end `types.c` linkage.
 * @details Iterates through typedef/typealias `tk_typeref` wrappers to reach the underlying type.
 * @param[in] ty Input EDG type pointer (may be null).
 * @param[out] out_ty Receives the underlying non-typeref `a_type_ptr` (or `nullptr`).
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t unwrap_omp_typerefs(
    a_type_ptr ty,
    a_type_ptr* out_ty) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    while (ty && ty->kind == tk_typeref) {
        ty = ty->variant.typeref.type;
    }
    *out_ty = ty;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests whether a character is valid inside a C/C++ identifier token.
 * @details Writes `true` into @p out_is_ident for ASCII alphanumeric characters, `'_'`, and `'$'`.
 * @param[in] ch Character to inspect.
 * @param[out] out_is_ident Receives `true` if @p ch can belong to an identifier token.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t check_omp_ident_char(
    char ch,
    bool* out_is_ident) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    *out_is_ident = (std::isalnum(static_cast<unsigned char>(ch)) != 0) || ch == '_' || ch == '$';
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Checks whether an exact identifier token appears in a comma-separated OpenMP list string.
 * @details Scans @p list_str enforcing non-identifier boundaries immediately before and after @p var_name.
 * @param[in] list_str Substring inside parentheses (after any `modifier:` prefix).
 * @param[in] var_name Target variable identifier.
 * @param[out] out_contains Receives `true` if @p var_name appears as a complete token in @p list_str.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t omp_list_contains_ident(
    llvm::StringRef list_str,
    llvm::StringRef var_name,
    bool* out_contains) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    *out_contains = false;
    size_t search_from = 0;
    while (search_from < list_str.size()) {
        size_t hit = list_str.find(var_name, search_from);
        if (hit == llvm::StringRef::npos) return llvm_gen_be_error_t::ok;
        bool left_ident = false;
        if (hit > 0) {
            LLVM_GEN_BE_CHECK(check_omp_ident_char(list_str[hit - 1], &left_ident));
        }
        size_t after = hit + var_name.size();
        bool right_ident = false;
        if (after < list_str.size()) {
            LLVM_GEN_BE_CHECK(check_omp_ident_char(list_str[after], &right_ident));
        }
        if (!left_ident && !right_ident) {
            *out_contains = true;
            return llvm_gen_be_error_t::ok;
        }
        search_from = hit + 1;
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Checks whether a variable name appears inside a specific OpenMP clause in directive text.
 * @details Searches for `clause_name(` (e.g., `"firstprivate("` or `"private("`) on exact token
 *          boundaries and checks if `var_name` appears as a complete identifier token in the list.
 * @param[in] directive_text Full OpenMP pragma string (may be null).
 * @param[in] clause_name Name of the clause without parentheses (e.g., `"private"`).
 * @param[in] var_name Variable identifier to check.
 * @param[out] out_found Receives `true` if `var_name` is present in the clause.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t is_var_in_omp_clause(
    const char* directive_text,
    const char* clause_name,
    const char* var_name,
    bool* out_found) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    *out_found = false;
    if (!directive_text || var_name[0] == '\0') {
        return llvm_gen_be_error_t::ok;
    }
    std::string text = std::string(directive_text);
    std::string pattern = std::string(clause_name) + "(";
    size_t pos = 0;
    while ((pos = text.find(pattern, pos)) != std::string::npos) {
        bool prev_ident = false;
        if (pos > 0) {
            LLVM_GEN_BE_CHECK(check_omp_ident_char(text[pos - 1], &prev_ident));
        }
        if (prev_ident) {
            pos += pattern.size();
            continue;
        }
        size_t start = pos + pattern.size();
        size_t end = text.find(')', start);
        if (end == std::string::npos) {
            break;
        }
        llvm::StringRef list_ref = llvm::StringRef(text).slice(start, end);
        size_t colon = list_ref.find(':');
        if (colon != llvm::StringRef::npos) {
            list_ref = list_ref.substr(colon + 1);
        }
        bool matched = false;
        LLVM_GEN_BE_CHECK(omp_list_contains_ident(list_ref, var_name, &matched));
        if (matched) {
            *out_found = true;
            return llvm_gen_be_error_t::ok;
        }
        pos = end + 1;
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Checks whether a variable participates in an OpenMP `reduction(op: ...)` clause.
 * @details Scans all `reduction(...)` clauses in @p directive_text, extracts the reduction operator
 *          token before `:`, and tests whether @p var_name is listed by exact identifier boundary.
 * @param[in] directive_text Full OpenMP pragma string (may be null).
 * @param[in] var_name Variable identifier to search for.
 * @param[out] out_op Receives the trimmed reduction operator string (e.g. `"+"`, `"*"`, `"min"`).
 * @param[out] out_found Receives `true` if @p var_name is in a `reduction` clause.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t find_var_in_omp_reduction(
    const char* directive_text,
    const char* var_name,
    std::string* out_op,
    bool* out_found) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    *out_found = false;
    out_op->clear();
    if (!directive_text || var_name[0] == '\0') {
        return llvm_gen_be_error_t::ok;
    }
    llvm::StringRef text = llvm::StringRef(directive_text);
    size_t pos = 0;
    while ((pos = text.find("reduction(", pos)) != llvm::StringRef::npos) {
        bool prev_ident = false;
        if (pos > 0) {
            LLVM_GEN_BE_CHECK(check_omp_ident_char(text[pos - 1], &prev_ident));
        }
        if (prev_ident) {
            pos += 10;
            continue;
        }
        size_t start = pos + 10;
        size_t end = text.find(')', start);
        if (end == llvm::StringRef::npos) break;
        llvm::StringRef inner = text.slice(start, end);
        size_t colon = inner.rfind(':');
        if (colon != llvm::StringRef::npos) {
            llvm::StringRef op_part = inner.slice(0, colon).trim();
            size_t comma_mod = op_part.rfind(',');
            if (comma_mod != llvm::StringRef::npos) {
                op_part = op_part.substr(comma_mod + 1).trim();
            }
            llvm::StringRef vars_part = inner.substr(colon + 1);
            bool matched = false;
            LLVM_GEN_BE_CHECK(omp_list_contains_ident(vars_part, var_name, &matched));
            if (matched) {
                *out_op = op_part.str();
                *out_found = true;
                return llvm_gen_be_error_t::ok;
            }
        }
        pos = end + 1;
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Initializes a `private` or `firstprivate` variable slot, invoking C++ constructors when present.
 * @details Inspects class scope routines for default constructors (`private`) or copy constructors
 *          (`firstprivate`), falling back to zero-init or bitwise load/store for scalar/trivial types,
 *          and records any non-trivial class destructor in @p out_dtor.
 * @param[in] var EDG variable being privatized.
 * @param[in] var_ty Lowered LLVM type of @p var.
 * @param[in] priv_alloca Thread-private stack allocation in the microtask.
 * @param[in] src_ptr Shared parent storage pointer passed into the microtask.
 * @param[in] is_firstpriv `true` for `firstprivate`, `false` for `private`.
 * @param[out] out_dtor Receives the class destructor routine if @p var requires destruction at exit.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t emit_omp_private_var_init(
    a_variable_ptr var,
    llvm::Type* var_ty,
    llvm::AllocaInst* priv_alloca,
    llvm::Value* src_ptr,
    bool is_firstpriv,
    a_routine_ptr* out_dtor) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    *out_dtor = nullptr;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    a_type_ptr ut = nullptr;
    LLVM_GEN_BE_CHECK(unwrap_omp_typerefs(var->type, &ut));

    if (ut && (ut->kind == tk_class || ut->kind == tk_struct)) {
        a_routine_ptr def_ctor = nullptr;
        a_routine_ptr copy_ctor = nullptr;
        auto* extra = ut->variant.class_struct_union.extra_info;
        if (extra && extra->assoc_scope) {
            for (a_routine_ptr r = extra->assoc_scope->routines; r != nullptr; r = r->next) {
                if (r->special_kind == sfk_destructor && !r->compiler_generated) {
                    *out_dtor = r;
                } else if (r->special_kind == sfk_constructor && !r->is_deleted) {
                    a_type_ptr rt = nullptr;
                    LLVM_GEN_BE_CHECK(unwrap_omp_typerefs(r->type, &rt));
                    a_param_type_ptr pt = (rt && rt->kind == tk_routine && rt->variant.routine.extra_info)
                                              ? rt->variant.routine.extra_info->param_type_list
                                              : nullptr;
                    if (!pt) {
                        if (!r->compiler_generated || !def_ctor) def_ctor = r;
                    } else if (!pt->next) {
                        a_type_ptr p0 = nullptr;
                        LLVM_GEN_BE_CHECK(unwrap_omp_typerefs(pt->type, &p0));
                        if (p0 && p0->kind == tk_pointer && p0->variant.pointer.is_reference) {
                            if (!r->compiler_generated || !copy_ctor) copy_ctor = r;
                        }
                    }
                }
            }
        }
        if (is_firstpriv && copy_ctor && copy_ctor->source_corresp.name) {
            llvm::Function* fn = nullptr;
            LLVM_GEN_BE_CHECK(llvm_get_or_declare_routine(copy_ctor, &fn));
            if (fn->arg_size() >= 2) {
                be_state->builder->CreateCall(fn, {priv_alloca, src_ptr});
            } else {
                llvm::FunctionType* cty = llvm::FunctionType::get(void_ty, {ptr_ty, ptr_ty}, false);
                llvm::FunctionCallee callee = be_state->module->getOrInsertFunction(copy_ctor->source_corresp.name, cty);
                be_state->builder->CreateCall(callee, {priv_alloca, src_ptr});
            }
            return llvm_gen_be_error_t::ok;
        }
        if (!is_firstpriv && def_ctor && def_ctor->source_corresp.name) {
            llvm::Function* fn = nullptr;
            LLVM_GEN_BE_CHECK(llvm_get_or_declare_routine(def_ctor, &fn));
            if (fn->arg_size() >= 1) {
                be_state->builder->CreateCall(fn, {priv_alloca});
            } else {
                llvm::FunctionType* dty = llvm::FunctionType::get(void_ty, {ptr_ty}, false);
                llvm::FunctionCallee callee = be_state->module->getOrInsertFunction(def_ctor->source_corresp.name, dty);
                be_state->builder->CreateCall(callee, {priv_alloca});
            }
            return llvm_gen_be_error_t::ok;
        }
    }

    if (is_firstpriv) {
        llvm::Value* init_val = be_state->builder->CreateLoad(var_ty, src_ptr, "firstpriv.init");
        be_state->builder->CreateStore(init_val, priv_alloca);
    } else {
        be_state->builder->CreateStore(llvm::Constant::getNullValue(var_ty), priv_alloca);
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Computes the OpenMP reduction identity constant for a scalar type and reduction operator.
 * @details Implements OpenMP 5.x Table 5.5 identity values (`0`, `1`, `~0`, `MAX`, `MIN`) across
 *          signed/unsigned integers and IEEE floating-point types.
 * @param[in] var_ty LLVM scalar type of the reduction variable.
 * @param[in] is_signed_int `true` if @p var_ty represents a signed integer type.
 * @param[in] op_str Trimmed OpenMP reduction operator string (`"+"`, `"-"`, `"*"`, `"&"`, `"|"`, `"^"`, `"&&"`, `"||"`, `"min"`, `"max"`).
 * @param[out] out_identity Receives the identity `llvm::Constant*` of type @p var_ty.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t get_omp_reduction_identity(
    llvm::Type* var_ty,
    bool is_signed_int,
    llvm::StringRef op_str,
    llvm::Constant** out_identity) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    if (var_ty->isFloatingPointTy()) {
        if (op_str == "*" || op_str == "&&") {
            *out_identity = llvm::ConstantFP::get(var_ty, 1.0);
        } else if (op_str == "min") {
            *out_identity = llvm::ConstantFP::get(
                var_ty->getContext(), llvm::APFloat::getLargest(var_ty->getFltSemantics(), false));
        } else if (op_str == "max") {
            *out_identity = llvm::ConstantFP::get(
                var_ty->getContext(), llvm::APFloat::getLargest(var_ty->getFltSemantics(), true));
        } else {
            *out_identity = llvm::ConstantFP::get(var_ty, 0.0);
        }
        return llvm_gen_be_error_t::ok;
    }
    if (llvm::IntegerType* int_ty = llvm::dyn_cast<llvm::IntegerType>(var_ty)) {
        unsigned bw = int_ty->getBitWidth();
        if (op_str == "*" || op_str == "&&") {
            *out_identity = llvm::ConstantInt::get(int_ty, 1);
        } else if (op_str == "&") {
            *out_identity = llvm::Constant::getAllOnesValue(int_ty);
        } else if (op_str == "min") {
            *out_identity = llvm::ConstantInt::get(
                var_ty->getContext(),
                is_signed_int ? llvm::APInt::getSignedMaxValue(bw) : llvm::APInt::getMaxValue(bw));
        } else if (op_str == "max") {
            *out_identity = llvm::ConstantInt::get(
                var_ty->getContext(),
                is_signed_int ? llvm::APInt::getSignedMinValue(bw) : llvm::APInt::getMinValue(bw));
        } else {
            *out_identity = llvm::ConstantInt::get(int_ty, 0);
        }
        return llvm_gen_be_error_t::ok;
    }
    *out_identity = llvm::Constant::getNullValue(var_ty);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits binary combination of two scalar OpenMP reduction values using @p builder.
 * @details Supports `+`, `-` (partial-sum combine is addition), `*`, `&`, `|`, `^`, `&&`, `||`,
 *          `min`, and `max` on both integer and floating-point types.
 * @param[in,out] builder IRBuilder positioned at the combine insertion point.
 * @param[in] lhs Left-hand accumulator value.
 * @param[in] rhs Right-hand partial value.
 * @param[in] is_signed_int `true` when integer `min`/`max` comparisons are signed.
 * @param[in] op_str Reduction operator token.
 * @param[out] out_val Receives the combined `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t emit_omp_reduction_binop(
    llvm::IRBuilder<>& builder,
    llvm::Value* lhs,
    llvm::Value* rhs,
    bool is_signed_int,
    llvm::StringRef op_str,
    llvm::Value** out_val) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    llvm::Type* ty = lhs->getType();
    if (ty->isFloatingPointTy()) {
        if (op_str == "*") {
            *out_val = builder.CreateFMul(lhs, rhs, "red.fmul");
        } else if (op_str == "min") {
            llvm::Value* cmp = builder.CreateFCmpOLT(lhs, rhs, "red.fmin.cmp");
            *out_val = builder.CreateSelect(cmp, lhs, rhs, "red.fmin");
        } else if (op_str == "max") {
            llvm::Value* cmp = builder.CreateFCmpOGT(lhs, rhs, "red.fmax.cmp");
            *out_val = builder.CreateSelect(cmp, lhs, rhs, "red.fmax");
        } else {
            *out_val = builder.CreateFAdd(lhs, rhs, "red.fadd");
        }
        return llvm_gen_be_error_t::ok;
    }
    if (op_str == "*") {
        *out_val = builder.CreateMul(lhs, rhs, "red.mul");
    } else if (op_str == "&") {
        *out_val = builder.CreateAnd(lhs, rhs, "red.and");
    } else if (op_str == "|") {
        *out_val = builder.CreateOr(lhs, rhs, "red.or");
    } else if (op_str == "^") {
        *out_val = builder.CreateXor(lhs, rhs, "red.xor");
    } else if (op_str == "&&") {
        llvm::Value* zero = llvm::Constant::getNullValue(ty);
        llvm::Value* l_nz = builder.CreateICmpNE(lhs, zero);
        llvm::Value* r_nz = builder.CreateICmpNE(rhs, zero);
        *out_val = builder.CreateIntCast(builder.CreateAnd(l_nz, r_nz), ty, false, "red.land");
    } else if (op_str == "||") {
        llvm::Value* zero = llvm::Constant::getNullValue(ty);
        llvm::Value* l_nz = builder.CreateICmpNE(lhs, zero);
        llvm::Value* r_nz = builder.CreateICmpNE(rhs, zero);
        *out_val = builder.CreateIntCast(builder.CreateOr(l_nz, r_nz), ty, false, "red.lor");
    } else if (op_str == "min") {
        llvm::Value* cmp = is_signed_int ? builder.CreateICmpSLT(lhs, rhs) : builder.CreateICmpULT(lhs, rhs);
        *out_val = builder.CreateSelect(cmp, lhs, rhs, "red.imin");
    } else if (op_str == "max") {
        llvm::Value* cmp = is_signed_int ? builder.CreateICmpSGT(lhs, rhs) : builder.CreateICmpUGT(lhs, rhs);
        *out_val = builder.CreateSelect(cmp, lhs, rhs, "red.imax");
    } else {
        *out_val = builder.CreateAdd(lhs, rhs, "red.add");
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Combines a thread-private reduction accumulator into the shared reduction variable via `libomp`.
 * @details Synthesizes a record-pointer combiner callback compatible with `__kmpc_reduce_nowait`,
 *          handles primary-thread combine (`case 1` + `__kmpc_end_reduce_nowait`) and atomic/CAS
 *          fallback (`case 2`) across all OpenMP arithmetic, bitwise, logical, and min/max operators.
 * @param[in] pos Source position for the OpenMP `ident_t` descriptor.
 * @param[in] var_ty LLVM type of the reduction variable.
 * @param[in] is_signed_int `true` if integer comparisons use signed predicates.
 * @param[in] op_str Reduction operator string.
 * @param[in] priv_alloca Thread-local accumulator alloca.
 * @param[in] shared_ptr Pointer to the shared parent reduction variable.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t emit_omp_var_reduction_combine(
    a_source_position pos,
    llvm::Type* var_ty,
    bool is_signed_int,
    llvm::StringRef op_str,
    llvm::AllocaInst* priv_alloca,
    llvm::Value* shared_ptr) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);

    static uint64_t s_red_comb_id = 0;
    std::string comb_name = ".omp.red.comb." + std::to_string(++s_red_comb_id);
    llvm::FunctionType* comb_fty = llvm::FunctionType::get(void_ty, {ptr_ty, ptr_ty}, false);
    llvm::Function* comb_fn = llvm::Function::Create(
        comb_fty, llvm::GlobalValue::InternalLinkage, comb_name, M);
    llvm::BasicBlock* comb_bb = llvm::BasicBlock::Create(Ctx, "entry", comb_fn);
    llvm::IRBuilder<> cb(comb_bb);
    llvm::Value* lhs_ptr = cb.CreateLoad(ptr_ty, comb_fn->getArg(0), "lhs.ptr");
    llvm::Value* rhs_ptr = cb.CreateLoad(ptr_ty, comb_fn->getArg(1), "rhs.ptr");
    llvm::Value* lhs_v = cb.CreateLoad(var_ty, lhs_ptr, "lhs.v");
    llvm::Value* rhs_v = cb.CreateLoad(var_ty, rhs_ptr, "rhs.v");
    llvm::Value* comb_v = nullptr;
    LLVM_GEN_BE_CHECK(emit_omp_reduction_binop(cb, lhs_v, rhs_v, is_signed_int, op_str, &comb_v));
    cb.CreateStore(comb_v, lhs_ptr);
    cb.CreateRetVoid();

    llvm::ArrayType* lock_arr_ty = llvm::ArrayType::get(i32_ty, 8);
    llvm::GlobalVariable* red_lock = M->getGlobalVariable(".omp.reduction.lock", true);
    if (!red_lock) {
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(
            M, lock_arr_ty, false, llvm::GlobalValue::InternalLinkage,
            llvm::ConstantAggregateZero::get(lock_arr_ty), ".omp.reduction.lock", &red_lock));
    }

    llvm::StructType* rec_ty = llvm::StructType::get(Ctx, llvm::ArrayRef<llvm::Type*>(ptr_ty));
    llvm::AllocaInst* red_rec = be_state->builder->CreateAlloca(rec_ty, nullptr, ".omp.red.rec");
    llvm::Value* rec_slot0 = be_state->builder->CreateStructGEP(rec_ty, red_rec, 0);
    be_state->builder->CreateStore(priv_alloca, rec_slot0);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, pos, &ident));
    llvm::Value* gtid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &gtid));

    llvm::FunctionType* reduce_ty = llvm::FunctionType::get(
        i32_ty, {ptr_ty, i32_ty, i32_ty, i64_ty, ptr_ty, ptr_ty, ptr_ty}, false);
    llvm::FunctionCallee reduce_nowait = M->getOrInsertFunction("__kmpc_reduce_nowait", reduce_ty);
    llvm::FunctionType* end_reduce_ty = llvm::FunctionType::get(void_ty, {ptr_ty, i32_ty, ptr_ty}, false);
    llvm::FunctionCallee end_reduce_nowait = M->getOrInsertFunction("__kmpc_end_reduce_nowait", end_reduce_ty);

    uint64_t rec_bytes = M->getDataLayout().getTypeAllocSize(rec_ty);
    llvm::Value* red_code = be_state->builder->CreateCall(
        reduce_nowait,
        {ident, gtid, llvm::ConstantInt::get(i32_ty, 1),
         llvm::ConstantInt::get(i64_ty, rec_bytes), red_rec, comb_fn, red_lock});

    llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* case1_bb = llvm::BasicBlock::Create(Ctx, "omp.red.case1", parent_fn);
    llvm::BasicBlock* case2_bb = llvm::BasicBlock::Create(Ctx, "omp.red.case2", parent_fn);
    llvm::BasicBlock* done_bb = llvm::BasicBlock::Create(Ctx, "omp.red.done", parent_fn);

    llvm::SwitchInst* sw = be_state->builder->CreateSwitch(red_code, done_bb, 2);
    sw->addCase(llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(i32_ty), 1), case1_bb);
    sw->addCase(llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(i32_ty), 2), case2_bb);

    be_state->builder->SetInsertPoint(case1_bb);
    llvm::Value* cur_sh = be_state->builder->CreateLoad(var_ty, shared_ptr, "red.sh.cur");
    llvm::Value* master_part = be_state->builder->CreateLoad(var_ty, priv_alloca, "red.master.part");
    llvm::Value* merged = nullptr;
    LLVM_GEN_BE_CHECK(emit_omp_reduction_binop(*be_state->builder, cur_sh, master_part, is_signed_int, op_str, &merged));
    be_state->builder->CreateStore(merged, shared_ptr);
    be_state->builder->CreateCall(end_reduce_nowait, {ident, gtid, red_lock});
    be_state->builder->CreateBr(done_bb);

    be_state->builder->SetInsertPoint(case2_bb);
    llvm::Value* thr_part = be_state->builder->CreateLoad(var_ty, priv_alloca, "red.thr.part");
    uint64_t val_align = M->getDataLayout().getABITypeAlign(var_ty).value();
    if (var_ty->isIntegerTy() && (op_str == "+" || op_str == "-" || op_str == "&" ||
                                  op_str == "|" || op_str == "^" || op_str == "min" || op_str == "max")) {
        llvm::AtomicRMWInst::BinOp rmw_op = llvm::AtomicRMWInst::Add;
        if (op_str == "&") rmw_op = llvm::AtomicRMWInst::And;
        else if (op_str == "|") rmw_op = llvm::AtomicRMWInst::Or;
        else if (op_str == "^") rmw_op = llvm::AtomicRMWInst::Xor;
        else if (op_str == "min") rmw_op = is_signed_int ? llvm::AtomicRMWInst::Min : llvm::AtomicRMWInst::UMin;
        else if (op_str == "max") rmw_op = is_signed_int ? llvm::AtomicRMWInst::Max : llvm::AtomicRMWInst::UMax;
        be_state->builder->CreateAtomicRMW(
            rmw_op, shared_ptr, thr_part, llvm::MaybeAlign(val_align), llvm::AtomicOrdering::Monotonic);
        be_state->builder->CreateBr(done_bb);
    } else {
        uint64_t bit_w = M->getDataLayout().getTypeSizeInBits(var_ty);
        llvm::IntegerType* cas_int_ty = llvm::Type::getIntNTy(Ctx, static_cast<unsigned>(bit_w));
        llvm::BasicBlock* cas_hdr = llvm::BasicBlock::Create(Ctx, "omp.red.cas", parent_fn);
        llvm::Value* init_raw = be_state->builder->CreateLoad(cas_int_ty, shared_ptr, "red.cas.init");
        llvm::BasicBlock* cas_pred = be_state->builder->GetInsertBlock();
        be_state->builder->CreateBr(cas_hdr);

        be_state->builder->SetInsertPoint(cas_hdr);
        llvm::PHINode* old_raw = be_state->builder->CreatePHI(cas_int_ty, 2, "red.cas.old");
        old_raw->addIncoming(init_raw, cas_pred);
        llvm::Value* old_typed = (var_ty == cas_int_ty)
                                     ? static_cast<llvm::Value*>(old_raw)
                                     : be_state->builder->CreateBitCast(old_raw, var_ty);
        llvm::Value* next_typed = nullptr;
        LLVM_GEN_BE_CHECK(emit_omp_reduction_binop(
            *be_state->builder, old_typed, thr_part, is_signed_int, op_str, &next_typed));
        llvm::Value* next_raw = (var_ty == cas_int_ty)
                                    ? next_typed
                                    : be_state->builder->CreateBitCast(next_typed, cas_int_ty);
        llvm::AtomicCmpXchgInst* cmpx = be_state->builder->CreateAtomicCmpXchg(
            shared_ptr, old_raw, next_raw, llvm::MaybeAlign(val_align),
            llvm::AtomicOrdering::Monotonic, llvm::AtomicOrdering::Monotonic);
        llvm::Value* loaded_raw = be_state->builder->CreateExtractValue(cmpx, 0);
        llvm::Value* ok_flag = be_state->builder->CreateExtractValue(cmpx, 1);
        old_raw->addIncoming(loaded_raw, cas_hdr);
        be_state->builder->CreateCondBr(ok_flag, done_bb, cas_hdr);
    }

    be_state->builder->SetInsertPoint(done_bb);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP parallel region (including combined `parallel for` / `parallel sections`) into LLVM IR.
 * @details Outlines the parallel body into `.omp_microtask.`, captures active local variables
 *          (`shared`, `private`, `firstprivate`, `reduction`), invokes C++ constructors/destructors
 *          for privatized class objects, dispatches combined worksharing loops/sections inside the
 *          microtask, combines reductions, and emits `__kmpc_fork_call`.
 * @param[in] stmt The EDG statement representing the parallel region.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_parallel_region(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    const char* directive_text = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_directive_text(stmt, &directive_text));
    std::string dir_str = directive_text ? directive_text : "";
    bool is_combined_for = (dir_str.find("omp parallel for") != std::string::npos);
    bool is_combined_sections = (dir_str.find("omp parallel sections") != std::string::npos);

    omp_capture_context_t cap_ctx;
    std::vector<std::string> red_ops;
    for (const auto& kv : be_state->local_vars) {
        if (kv.first && kv.second) {
            const char* vname = kv.first->source_corresp.name ? kv.first->source_corresp.name : "";
            bool is_firstpriv = false;
            bool is_priv = false;
            bool is_red = false;
            std::string red_op;
            LLVM_GEN_BE_CHECK(is_var_in_omp_clause(directive_text, "firstprivate", vname, &is_firstpriv));
            LLVM_GEN_BE_CHECK(is_var_in_omp_clause(directive_text, "private", vname, &is_priv));
            LLVM_GEN_BE_CHECK(find_var_in_omp_reduction(directive_text, vname, &red_op, &is_red));
            omp_capture_attr_t attr = omp_capture_attr_t::shared;
            if (is_red) attr = omp_capture_attr_t::reduction;
            else if (is_firstpriv) attr = omp_capture_attr_t::firstprivate;
            else if (is_priv) attr = omp_capture_attr_t::priv;
            cap_ctx.vars.push_back(kv.first);
            cap_ctx.parent_ptrs.push_back(kv.second);
            cap_ctx.share_attrs.push_back(attr);
            red_ops.push_back(red_op);
        }
    }

    std::vector<llvm::Type*> microtask_params = {ptr_ty, ptr_ty};
    for (size_t i = 0; i < cap_ctx.vars.size(); ++i) {
        microtask_params.push_back(ptr_ty);
    }

    llvm::FunctionType* microtask_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        microtask_params,
        true
    );
    
    llvm::Function* microtask = llvm::Function::Create(
        microtask_ty, llvm::GlobalValue::InternalLinkage, ".omp_microtask.", M);
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(Ctx, "entry", microtask);
    
    llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
    auto saved_locals = be_state->local_vars;
    be_state->builder->SetInsertPoint(entry);

    /**
     * @struct PrivatizedDtorItem
     * @brief Tracks a privatized C++ object slot requiring destruction at microtask exit.
     */
    struct PrivatizedDtorItem {
        a_routine_ptr dtor;     ///< Class destructor routine to invoke.
        llvm::AllocaInst* slot; ///< Thread-private stack slot to destroy.
    };
    /**
     * @struct ReductionItem
     * @brief Tracks a thread-private OpenMP reduction accumulator and its shared parent target.
     */
    struct ReductionItem {
        llvm::Type* var_ty;          ///< LLVM scalar type of the reduction variable.
        bool is_signed;              ///< True if signed integer predicates apply.
        std::string op;              ///< Reduction operator token string.
        llvm::AllocaInst* priv_slot; ///< Thread-local accumulator stack slot.
        llvm::Value* shared_arg;     ///< Pointer to shared parent reduction storage.
    };
    std::vector<PrivatizedDtorItem> priv_dtors;
    std::vector<ReductionItem> active_reductions;

    for (size_t i = 0; i < cap_ctx.vars.size(); ++i) {
        a_variable_ptr var = cap_ctx.vars[i];
        llvm::Argument* arg_ptr = microtask->getArg(static_cast<unsigned>(2 + i));
        const char* vname = var->source_corresp.name ? var->source_corresp.name : "";
        omp_capture_attr_t attr = cap_ctx.share_attrs[i];
        bool is_firstpriv = (attr == omp_capture_attr_t::firstprivate);
        bool is_priv = (attr == omp_capture_attr_t::priv);
        bool is_red = (attr == omp_capture_attr_t::reduction);

        if (is_firstpriv || is_priv || is_red) {
            llvm::Type* var_ty = llvm::Type::getInt32Ty(Ctx);
            if (var->type) {
                llvm_gen_be_error_t ty_err = get_llvm_type(var->type, &var_ty);
                if (ty_err != llvm_gen_be_error_t::ok) {
                    be_state->local_vars = std::move(saved_locals);
                    be_state->builder->restoreIP(saved_ip);
                    return ty_err;
                }
            }
            if (is_red) {
                a_type_ptr ut = nullptr;
                llvm_gen_be_error_t u_err = unwrap_omp_typerefs(var->type, &ut);
                if (u_err != llvm_gen_be_error_t::ok) {
                    be_state->local_vars = std::move(saved_locals);
                    be_state->builder->restoreIP(saved_ip);
                    return u_err;
                }
                bool is_signed = true;
                if (ut && ut->kind == tk_integer) {
                    is_signed = (int_kind_is_signed[ut->variant.integer.int_kind] != FALSE);
                }
                llvm::AllocaInst* red_alloca = be_state->builder->CreateAlloca(
                    var_ty, nullptr, std::string(vname) + ".red");
                llvm::Constant* red_id = nullptr;
                llvm_gen_be_error_t id_err = get_omp_reduction_identity(var_ty, is_signed, red_ops[i], &red_id);
                if (id_err != llvm_gen_be_error_t::ok) {
                    be_state->local_vars = std::move(saved_locals);
                    be_state->builder->restoreIP(saved_ip);
                    return id_err;
                }
                be_state->builder->CreateStore(red_id, red_alloca);
                be_state->local_vars[var] = red_alloca;
                active_reductions.push_back({var_ty, is_signed, red_ops[i], red_alloca, arg_ptr});
            } else {
                llvm::AllocaInst* priv_alloca = be_state->builder->CreateAlloca(
                    var_ty, nullptr, std::string(vname) + (is_firstpriv ? ".firstpriv" : ".priv"));
                a_routine_ptr dtor_rt = nullptr;
                llvm_gen_be_error_t init_err = emit_omp_private_var_init(
                    var, var_ty, priv_alloca, arg_ptr, is_firstpriv, &dtor_rt);
                if (init_err != llvm_gen_be_error_t::ok) {
                    be_state->local_vars = std::move(saved_locals);
                    be_state->builder->restoreIP(saved_ip);
                    return init_err;
                }
                if (dtor_rt) {
                    priv_dtors.push_back({dtor_rt, priv_alloca});
                }
                be_state->local_vars[var] = priv_alloca;
            }
        } else {
            be_state->local_vars[var] = arg_ptr;
        }
    }
    
    llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;
    if (is_combined_for) {
        err = lower_omp_for_worksharing(stmt);
        if (err == llvm_gen_be_error_t::ok && dir_str.find("omp parallel for simd") != std::string::npos) {
            err = lower_omp_simd(stmt);
        }
    } else if (is_combined_sections) {
        err = lower_omp_sections(stmt);
    } else {
        err = llvm_lower_statement(stmt);
    }
    if (err != llvm_gen_be_error_t::ok) {
        be_state->local_vars = std::move(saved_locals);
        be_state->builder->restoreIP(saved_ip);
        return err;
    }
    
    if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
        for (const ReductionItem& ri : active_reductions) {
            llvm_gen_be_error_t red_err = emit_omp_var_reduction_combine(
                stmt->position, ri.var_ty, ri.is_signed, ri.op, ri.priv_slot, ri.shared_arg);
            if (red_err != llvm_gen_be_error_t::ok) {
                be_state->local_vars = std::move(saved_locals);
                be_state->builder->restoreIP(saved_ip);
                return red_err;
            }
        }
        for (size_t di = priv_dtors.size(); di > 0; --di) {
            const PrivatizedDtorItem& pd = priv_dtors[di - 1];
            if (pd.dtor->source_corresp.name) {
                llvm::Function* dfn = nullptr;
                llvm_gen_be_error_t d_err = llvm_get_or_declare_routine(pd.dtor, &dfn);
                if (d_err != llvm_gen_be_error_t::ok) {
                    be_state->local_vars = std::move(saved_locals);
                    be_state->builder->restoreIP(saved_ip);
                    return d_err;
                }
                if (dfn->arg_size() >= 1) {
                    be_state->builder->CreateCall(dfn, {pd.slot});
                } else {
                    llvm::FunctionType* dfty = llvm::FunctionType::get(
                        llvm::Type::getVoidTy(Ctx), {ptr_ty}, false);
                    llvm::FunctionCallee dcallee = M->getOrInsertFunction(pd.dtor->source_corresp.name, dfty);
                    be_state->builder->CreateCall(dcallee, {pd.slot});
                }
            }
        }
        be_state->builder->CreateRetVoid();
    }
    be_state->local_vars = std::move(saved_locals);
    be_state->builder->restoreIP(saved_ip);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
    
    llvm::FunctionType* fork_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {ptr_ty, llvm::Type::getInt32Ty(Ctx), ptr_ty},
        true
    );
    llvm::FunctionCallee fork_call = M->getOrInsertFunction("__kmpc_fork_call", fork_ty);
    
    std::vector<llvm::Value*> fork_args = {
        ident,
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), static_cast<uint64_t>(cap_ctx.vars.size())),
        microtask
    };
    for (llvm::Value* p_ptr : cap_ctx.parent_ptrs) {
        fork_args.push_back(p_ptr);
    }
    be_state->builder->CreateCall(fork_call, fork_args);
    
    return llvm_gen_be_error_t::ok;
}

/**
 * @struct OmpCollapsedLoopDim
 * @brief Captures canonical loop bounds, step, induction variable, and trip count for one loop nest level.
 * @details Populated by `extract_omp_canonical_loop_dim` for 1D and `collapse(K)` worksharing loops.
 */
struct OmpCollapsedLoopDim {
    a_variable_ptr ind_var = nullptr; ///< Induction variable for this loop dimension.
    llvm::Value* var_slot = nullptr;  ///< Stack/shared pointer slot in `be_state->local_vars`.
    llvm::Type* var_ty = nullptr;     ///< LLVM type of the induction variable (`i32`, `i64`, or `ptr`).
    bool is_signed = true;            ///< True if signed integer/iterator difference.
    bool is_descending = false;       ///< True if condition is `>` or `>=` (descending loop).
    llvm::Value* lb_val = nullptr;    ///< Lower bound value (integer or pointer).
    llvm::Value* step_mag = nullptr;  ///< Positive step magnitude in `i64`.
    llvm::Value* trip_count = nullptr;///< Non-negative iteration trip count in `i64`.
    bool valid_canonical = false;     ///< True if `ind_var`, `var_slot`, condition, and body are all valid.
};

/**
 * @brief Extracts canonical loop bounds, step, and trip count from an EDG `stmk_for` statement.
 * @details Supports `int32_t`, `uint32_t`, `int64_t`, `size_t`, `ptrdiff_t`, and pointer/random-access
 *          iterator induction variables, `<`, `<=`, `>`, `>=`, and `!=` conditions, and `++i`, `i++`,
 *          `--i`, `i--`, `i += s`, `i -= s`, and `i = i + s` increment expressions.
 * @param[in] loop_stmt Pointer to the EDG `stmk_for` statement.
 * @param[out] out_dim Receives extracted bounds and trip-count values.
 * @return llvm_gen_be_error_t::ok on success, or an error code on expression lowering failure.
 */
static llvm_gen_be_error_t extract_omp_canonical_loop_dim(
    a_statement_ptr loop_stmt,
    OmpCollapsedLoopDim* out_dim) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    *out_dim = OmpCollapsedLoopDim{};

    a_for_loop_ptr fl = loop_stmt->variant.for_loop.extra_info;
    llvm::Value* raw_lb = nullptr;
    if (fl && fl->initialization) {
        a_statement_ptr init_s = fl->initialization;
        if (init_s->kind == stmk_init && init_s->variant.dynamic_init) {
            a_dynamic_init_ptr dip = init_s->variant.dynamic_init;
            out_dim->ind_var = dip->variable;
            if (dip->kind == dik_expression && dip->variant.expression) {
                LLVM_GEN_BE_CHECK(llvm_lower_expression(dip->variant.expression, &raw_lb));
            }
        } else if (init_s->kind == stmk_expr && init_s->expr &&
                   init_s->expr->kind == enk_operation &&
                   init_s->expr->variant.operation.kind == eok_assign) {
            an_expr_node_ptr lhs = init_s->expr->variant.operation.operands;
            an_expr_node_ptr rhs = lhs ? lhs->next : nullptr;
            if (lhs && lhs->kind == enk_variable) {
                out_dim->ind_var = lhs->variant.variable.ptr;
            }
            if (rhs) {
                LLVM_GEN_BE_CHECK(llvm_lower_expression(rhs, &raw_lb));
            }
        }
    }

    if (out_dim->ind_var) {
        auto it = be_state->local_vars.find(out_dim->ind_var);
        if (it != be_state->local_vars.end() && it->second) {
            out_dim->var_slot = it->second;
            if (llvm::AllocaInst* ai = llvm::dyn_cast<llvm::AllocaInst>(out_dim->var_slot)) {
                out_dim->var_ty = ai->getAllocatedType();
            } else if (out_dim->ind_var->type) {
                LLVM_GEN_BE_CHECK(get_llvm_type(out_dim->ind_var->type, &out_dim->var_ty));
            }
        }
        a_type_ptr ut = nullptr;
        LLVM_GEN_BE_CHECK(unwrap_omp_typerefs(out_dim->ind_var->type, &ut));
        if (ut && ut->kind == tk_integer) {
            out_dim->is_signed = (int_kind_is_signed[ut->variant.integer.int_kind] != FALSE);
        }
    }
    if (!out_dim->var_ty) {
        out_dim->var_ty = raw_lb ? raw_lb->getType() : llvm::Type::getInt32Ty(Ctx);
    }
    if (!raw_lb) {
        raw_lb = llvm::Constant::getNullValue(out_dim->var_ty);
    }
    out_dim->lb_val = raw_lb;

    llvm::Value* raw_ub = nullptr;
    an_expr_operator_kind cmp_op = eok_lt;
    bool has_cond_op2 = false;
    if (loop_stmt->expr && loop_stmt->expr->kind == enk_operation) {
        cmp_op = loop_stmt->expr->variant.operation.kind;
        an_expr_node_ptr op1 = loop_stmt->expr->variant.operation.operands;
        an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
        if (op2) {
            LLVM_GEN_BE_CHECK(llvm_lower_expression(op2, &raw_ub));
            has_cond_op2 = (raw_ub != nullptr);
        }
    }

    llvm::Value* step_mag = llvm::ConstantInt::get(i64_ty, 1);
    bool step_is_neg = (cmp_op == eok_gt || cmp_op == eok_ge);
    if (fl && fl->increment) {
        an_expr_node_ptr inc = fl->increment;
        if (inc->kind == enk_operation) {
            an_expr_operator_kind ik = inc->variant.operation.kind;
            an_expr_node_ptr iop1 = inc->variant.operation.operands;
            an_expr_node_ptr iop2 = iop1 ? iop1->next : nullptr;
            if (ik == eok_pre_incr || ik == eok_post_incr) {
                step_mag = llvm::ConstantInt::get(i64_ty, 1);
            } else if (ik == eok_pre_decr || ik == eok_post_decr) {
                step_mag = llvm::ConstantInt::get(i64_ty, 1);
                step_is_neg = true;
            } else if ((ik == eok_add_assign || ik == eok_subtract_assign || ik == eok_padd_assign || ik == eok_psubtract_assign) && iop2) {
                llvm::Value* sv = nullptr;
                LLVM_GEN_BE_CHECK(llvm_lower_expression(iop2, &sv));
                if (sv && sv->getType()->isIntegerTy()) {
                    step_mag = be_state->builder->CreateIntCast(sv, i64_ty, out_dim->is_signed);
                }
                if (ik == eok_subtract_assign || ik == eok_psubtract_assign) step_is_neg = true;
            } else if (ik == eok_assign && iop2 && iop2->kind == enk_operation) {
                an_expr_operator_kind rk = iop2->variant.operation.kind;
                an_expr_node_ptr rop1 = iop2->variant.operation.operands;
                an_expr_node_ptr rop2 = rop1 ? rop1->next : nullptr;
                if ((rk == eok_add || rk == eok_subtract || rk == eok_padd || rk == eok_psubtract) && rop2) {
                    llvm::Value* sv = nullptr;
                    LLVM_GEN_BE_CHECK(llvm_lower_expression(rop2, &sv));
                    if (sv && sv->getType()->isIntegerTy()) {
                        step_mag = be_state->builder->CreateIntCast(sv, i64_ty, out_dim->is_signed);
                    }
                    if (rk == eok_subtract || rk == eok_psubtract) step_is_neg = true;
                }
            } else {
                llvm::Value* dummy_inc = nullptr;
                LLVM_GEN_BE_CHECK(llvm_lower_expression(inc, &dummy_inc));
            }
        } else {
            llvm::Value* raw_inc = nullptr;
            LLVM_GEN_BE_CHECK(llvm_lower_expression(inc, &raw_inc));
            if (raw_inc && raw_inc->getType()->isIntegerTy()) {
                llvm::Value* cand = be_state->builder->CreateIntCast(raw_inc, i64_ty, false);
                llvm::Value* is_z = be_state->builder->CreateICmpEQ(cand, llvm::ConstantInt::get(i64_ty, 0));
                step_mag = be_state->builder->CreateSelect(is_z, llvm::ConstantInt::get(i64_ty, 1), cand);
            }
        }
    }
    out_dim->is_descending = step_is_neg;
    out_dim->step_mag = step_mag;

    if (has_cond_op2 && out_dim->var_slot && loop_stmt->variant.for_loop.statement) {
        out_dim->valid_canonical = true;
        auto to_i64 = [&](llvm::Value* v) noexcept -> llvm::Value* {
            if (v->getType()->isPointerTy()) {
                return be_state->builder->CreatePtrToInt(v, i64_ty);
            }
            if (v->getType()->isIntegerTy()) {
                return be_state->builder->CreateIntCast(v, i64_ty, out_dim->is_signed);
            }
            return llvm::ConstantInt::get(i64_ty, 0);
        };
        llvm::Value* lb64 = to_i64(raw_lb);
        llvm::Value* ub64 = to_i64(raw_ub);
        if (out_dim->var_ty->isPointerTy() && out_dim->ind_var->type) {
            a_type_ptr pt = nullptr;
            LLVM_GEN_BE_CHECK(unwrap_omp_typerefs(out_dim->ind_var->type, &pt));
            if (pt && pt->kind == tk_pointer && pt->variant.pointer.type) {
                llvm::Type* pointee_ty = nullptr;
                LLVM_GEN_BE_CHECK(get_llvm_type(pt->variant.pointer.type, &pointee_ty));
                uint64_t elem_sz = be_state->module->getDataLayout().getTypeAllocSize(pointee_ty);
                if (elem_sz > 1) {
                    step_mag = be_state->builder->CreateMul(step_mag, llvm::ConstantInt::get(i64_ty, elem_sz));
                    out_dim->step_mag = step_mag;
                }
            }
        }
        llvm::Value* zero64 = llvm::ConstantInt::get(i64_ty, 0);
        llvm::Value* one64 = llvm::ConstantInt::get(i64_ty, 1);
        llvm::Value* non_empty = nullptr;
        llvm::Value* span = nullptr;
        if (!step_is_neg) {
            if (cmp_op == eok_le) {
                non_empty = out_dim->is_signed ? be_state->builder->CreateICmpSLE(lb64, ub64)
                                               : be_state->builder->CreateICmpULE(lb64, ub64);
                span = be_state->builder->CreateSub(ub64, lb64);
            } else {
                non_empty = out_dim->is_signed ? be_state->builder->CreateICmpSLT(lb64, ub64)
                                               : be_state->builder->CreateICmpULT(lb64, ub64);
                span = be_state->builder->CreateSub(be_state->builder->CreateSub(ub64, lb64), one64);
            }
        } else {
            if (cmp_op == eok_ge) {
                non_empty = out_dim->is_signed ? be_state->builder->CreateICmpSGE(lb64, ub64)
                                               : be_state->builder->CreateICmpUGE(lb64, ub64);
                span = be_state->builder->CreateSub(lb64, ub64);
            } else {
                non_empty = out_dim->is_signed ? be_state->builder->CreateICmpSGT(lb64, ub64)
                                               : be_state->builder->CreateICmpUGT(lb64, ub64);
                span = be_state->builder->CreateSub(be_state->builder->CreateSub(lb64, ub64), one64);
            }
        }
        llvm::Value* trips = be_state->builder->CreateAdd(be_state->builder->CreateUDiv(span, step_mag), one64);
        out_dim->trip_count = be_state->builder->CreateSelect(non_empty, trips, zero64, ".omp.trips");
    } else {
        out_dim->trip_count = llvm::ConstantInt::get(i64_ty, 1);
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `for` worksharing construct into LLVM IR.
 * @details Supports 32-bit and 64-bit signed/unsigned induction variables and random-access
 *          iterators, `collapse(K)` multi-dimensional iteration space flattening with `/` and `%`
 *          index recovery, `lastprivate` writeback, standalone `for reduction(...)` clauses, and
 *          mutually exclusive static (`__kmpc_for_static_init_{4,4u,8,8u}`) vs dynamic/guided/runtime
 *          (`__kmpc_dispatch_init_{4,4u,8,8u}` / `__kmpc_dispatch_next_{4,4u,8,8u}`) loop protocols
 *          executing only the loop body per thread-assigned iteration.
 * @param[in] stmt The EDG statement representing the worksharing construct.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_for_worksharing(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);

    const char* directive = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_directive_text(stmt, &directive));
    std::string dir_str = directive ? directive : "";

    uint32_t collapse_k = 1;
    size_t col_pos = dir_str.find("collapse(");
    if (col_pos != std::string::npos) {
        unsigned long parsed_k = std::strtoul(dir_str.c_str() + col_pos + 9, nullptr, 10);
        if (parsed_k >= 1) {
            collapse_k = static_cast<uint32_t>(parsed_k);
        }
    }

    uint32_t sched_kind = 34; // kmp_sch_static
    uint64_t chunk_val = 1;
    bool has_explicit_chunk = false;
    bool is_dynamic_sched = false;
    size_t sched_pos = dir_str.find("schedule(");
    if (sched_pos != std::string::npos) {
        size_t sched_end = dir_str.find(')', sched_pos + 9);
        std::string sched_inner = (sched_end != std::string::npos)
                                      ? dir_str.substr(sched_pos + 9, sched_end - (sched_pos + 9))
                                      : dir_str.substr(sched_pos + 9);
        size_t comma = sched_inner.find(',');
        if (comma != std::string::npos) {
            unsigned long parsed_chunk = std::strtoul(sched_inner.c_str() + comma + 1, nullptr, 10);
            if (parsed_chunk >= 1) {
                chunk_val = parsed_chunk;
                has_explicit_chunk = true;
            }
        }
        if (sched_inner.find("dynamic") != std::string::npos) {
            sched_kind = 35;
            is_dynamic_sched = true;
        } else if (sched_inner.find("guided") != std::string::npos) {
            sched_kind = 36;
            is_dynamic_sched = true;
        } else if (sched_inner.find("runtime") != std::string::npos) {
            sched_kind = 37;
            is_dynamic_sched = true;
        } else if (sched_inner.find("auto") != std::string::npos) {
            sched_kind = 38;
            is_dynamic_sched = true;
        } else if (has_explicit_chunk) {
            sched_kind = 33; // kmp_sch_static_chunked
        }
    }

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));

    std::vector<OmpCollapsedLoopDim> dims;
    a_statement_ptr innermost_body = stmt;
    if (stmt->kind == stmk_for) {
        a_statement_ptr cur_loop = stmt;
        for (uint32_t d = 0; d < collapse_k && cur_loop && cur_loop->kind == stmk_for; ++d) {
            OmpCollapsedLoopDim dim;
            LLVM_GEN_BE_CHECK(extract_omp_canonical_loop_dim(cur_loop, &dim));
            dims.push_back(dim);
            innermost_body = cur_loop->variant.for_loop.statement;
            cur_loop = innermost_body;
        }
    }

    bool all_canonical = !dims.empty() && (innermost_body != nullptr);
    for (const OmpCollapsedLoopDim& d : dims) {
        if (!d.valid_canonical) {
            all_canonical = false;
            break;
        }
    }

    // Handle standalone `#pragma omp for reduction(...)` when not already privatized by enclosing `omp parallel`
    /**
     * @struct StandaloneForReduction
     * @brief Tracks a standalone `#pragma omp for reduction(...)` variable slot and saved binding.
     */
    struct StandaloneForReduction {
        a_variable_ptr var;          ///< EDG reduction variable pointer.
        llvm::Value* saved_slot;     ///< Original shared storage slot in `local_vars`.
        llvm::Type* var_ty;          ///< Lowered LLVM scalar type.
        bool is_signed;              ///< True when integer reduction predicates are signed.
        std::string op;              ///< Reduction operator token string.
        llvm::AllocaInst* priv_slot; ///< Thread-private accumulator stack slot.
    };
    std::vector<StandaloneForReduction> standalone_reds;
    if (dir_str.find("omp parallel") == std::string::npos && dir_str.find("reduction(") != std::string::npos) {
        for (const auto& kv : be_state->local_vars) {
            if (kv.first && kv.second && kv.first->source_corresp.name) {
                bool in_red = false;
                std::string rop;
                LLVM_GEN_BE_CHECK(find_var_in_omp_reduction(directive, kv.first->source_corresp.name, &rop, &in_red));
                if (in_red) {
                    llvm::Type* vty = i32_ty;
                    if (kv.first->type) {
                        LLVM_GEN_BE_CHECK(get_llvm_type(kv.first->type, &vty));
                    }
                    a_type_ptr ut = nullptr;
                    LLVM_GEN_BE_CHECK(unwrap_omp_typerefs(kv.first->type, &ut));
                    bool sgn = !(ut && ut->kind == tk_integer && int_kind_is_signed[ut->variant.integer.int_kind] == FALSE);
                    llvm::AllocaInst* rslot = be_state->builder->CreateAlloca(
                        vty, nullptr, std::string(kv.first->source_corresp.name) + ".for.red");
                    llvm::Constant* red_id = nullptr;
                    LLVM_GEN_BE_CHECK(get_omp_reduction_identity(vty, sgn, rop, &red_id));
                    be_state->builder->CreateStore(red_id, rslot);
                    standalone_reds.push_back({kv.first, kv.second, vty, sgn, rop, rslot});
                }
            }
        }
        for (const StandaloneForReduction& sr : standalone_reds) {
            be_state->local_vars[sr.var] = sr.priv_slot;
        }
    }

    auto restore_standalone_reds = [&](bool emit_combine) noexcept -> llvm_gen_be_error_t {
        LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
        for (const StandaloneForReduction& sr : standalone_reds) {
            be_state->local_vars[sr.var] = sr.saved_slot;
            if (emit_combine && !EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
                LLVM_GEN_BE_CHECK(emit_omp_var_reduction_combine(
                    stmt->position, sr.var_ty, sr.is_signed, sr.op, sr.priv_slot, sr.saved_slot));
            }
        }
        return llvm_gen_be_error_t::ok;
    };

    if (!all_canonical) {
        llvm::AllocaInst* plastiter = be_state->builder->CreateAlloca(i32_ty, nullptr, ".omp.is_last");
        llvm::AllocaInst* plower = be_state->builder->CreateAlloca(i32_ty, nullptr, ".omp.lb");
        llvm::AllocaInst* pupper = be_state->builder->CreateAlloca(i32_ty, nullptr, ".omp.ub");
        llvm::AllocaInst* pstride = be_state->builder->CreateAlloca(i32_ty, nullptr, ".omp.stride");
        be_state->builder->CreateStore(llvm::ConstantInt::get(i32_ty, 0), plastiter);
        be_state->builder->CreateStore(llvm::ConstantInt::get(i32_ty, 0), plower);
        be_state->builder->CreateStore(llvm::ConstantInt::get(i32_ty, 0), pupper);
        be_state->builder->CreateStore(llvm::ConstantInt::get(i32_ty, 1), pstride);

        if (!is_dynamic_sched) {
            llvm::FunctionType* init_ty = llvm::FunctionType::get(
                void_ty, {ptr_ty, i32_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, i32_ty, i32_ty}, false);
            llvm::FunctionCallee init_call = M->getOrInsertFunction("__kmpc_for_static_init_4", init_ty);
            be_state->builder->CreateCall(init_call, {
                ident, global_tid, llvm::ConstantInt::get(i32_ty, sched_kind),
                plastiter, plower, pupper, pstride,
                llvm::ConstantInt::get(i32_ty, 1), llvm::ConstantInt::get(i32_ty, chunk_val)
            });
        } else {
            llvm::FunctionType* dinit_ty = llvm::FunctionType::get(
                void_ty, {ptr_ty, i32_ty, i32_ty, i32_ty, i32_ty, i32_ty, i32_ty}, false);
            llvm::FunctionType* dnext_ty = llvm::FunctionType::get(
                i32_ty, {ptr_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty}, false);
            llvm::FunctionCallee dinit_call = M->getOrInsertFunction("__kmpc_dispatch_init_4", dinit_ty);
            llvm::FunctionCallee dnext_call = M->getOrInsertFunction("__kmpc_dispatch_next_4", dnext_ty);
            be_state->builder->CreateCall(dinit_call, {
                ident, global_tid, llvm::ConstantInt::get(i32_ty, sched_kind),
                llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, 0),
                llvm::ConstantInt::get(i32_ty, 1), llvm::ConstantInt::get(i32_ty, chunk_val)
            });
            be_state->builder->CreateCall(dnext_call, {ident, global_tid, plastiter, plower, pupper, pstride});
        }

        llvm_gen_be_error_t body_err = llvm_gen_be_error_t::ok;
        if (stmt->kind == stmk_for) {
            if (innermost_body) {
                body_err = llvm_lower_statement(innermost_body);
            }
        } else {
            body_err = llvm_lower_statement(stmt);
        }
        if (body_err != llvm_gen_be_error_t::ok) {
            LLVM_GEN_BE_CHECK(restore_standalone_reds(false));
            return body_err;
        }
        if (!is_dynamic_sched && !EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
            llvm::FunctionType* fini_ty = llvm::FunctionType::get(void_ty, {ptr_ty, i32_ty}, false);
            llvm::FunctionCallee fini_call = M->getOrInsertFunction("__kmpc_for_static_fini", fini_ty);
            be_state->builder->CreateCall(fini_call, {ident, global_tid});
        }
        return restore_standalone_reds(true);
    }

    bool use_64bit = (dims.size() > 1) || dims[0].var_ty->isPointerTy() ||
                     (dims[0].var_ty->isIntegerTy() && dims[0].var_ty->getIntegerBitWidth() > 32);
    bool use_signed = (dims.size() > 1) ? true : dims[0].is_signed;
    llvm::IntegerType* idx_ty = use_64bit ? llvm::Type::getInt64Ty(Ctx) : llvm::Type::getInt32Ty(Ctx);
    const char* suffix = use_64bit ? (use_signed ? "8" : "8u") : (use_signed ? "4" : "4u");

    llvm::Value* total_trips64 = dims[0].trip_count;
    for (size_t d = 1; d < dims.size(); ++d) {
        total_trips64 = be_state->builder->CreateMul(total_trips64, dims[d].trip_count, ".omp.collapse.prod");
    }

    llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* active_bb = llvm::BasicBlock::Create(Ctx, "omp.for.active", parent_fn);
    llvm::BasicBlock* done_bb = llvm::BasicBlock::Create(Ctx, "omp.for.done", parent_fn);
    llvm::Value* has_iters = be_state->builder->CreateICmpUGT(total_trips64, llvm::ConstantInt::get(i64_ty, 0));
    be_state->builder->CreateCondBr(has_iters, active_bb, done_bb);

    be_state->builder->SetInsertPoint(active_bb);
    llvm::Value* total_trips_idx = be_state->builder->CreateIntCast(total_trips64, idx_ty, false);
    llvm::Value* zero_idx = llvm::ConstantInt::get(idx_ty, 0);
    llvm::Value* one_idx = llvm::ConstantInt::get(idx_ty, 1);
    llvm::Value* chunk_idx = llvm::ConstantInt::get(idx_ty, chunk_val);
    llvm::Value* max_ub = be_state->builder->CreateSub(total_trips_idx, one_idx, ".omp.max_ub");

    llvm::AllocaInst* plastiter = be_state->builder->CreateAlloca(i32_ty, nullptr, ".omp.is_last");
    llvm::AllocaInst* plower = be_state->builder->CreateAlloca(idx_ty, nullptr, ".omp.lb");
    llvm::AllocaInst* pupper = be_state->builder->CreateAlloca(idx_ty, nullptr, ".omp.ub");
    llvm::AllocaInst* pstride = be_state->builder->CreateAlloca(idx_ty, nullptr, ".omp.stride");
    llvm::AllocaInst* iv_slot = be_state->builder->CreateAlloca(idx_ty, nullptr, ".omp.iv");
    be_state->builder->CreateStore(llvm::ConstantInt::get(i32_ty, 0), plastiter);

    auto emit_index_recovery_and_body = [&](llvm::BasicBlock* cont_target,
                                            llvm::BasicBlock* brk_target) noexcept -> llvm_gen_be_error_t {
        llvm::Value* iv_cur = be_state->builder->CreateLoad(idx_ty, iv_slot, ".omp.iv.cur");
        llvm::Value* rem64 = be_state->builder->CreateIntCast(iv_cur, i64_ty, false);
        for (size_t rev = dims.size(); rev > 0; --rev) {
            const OmpCollapsedLoopDim& d = dims[rev - 1];
            llvm::Value* coord64 = rem64;
            if (dims.size() > 1) {
                coord64 = be_state->builder->CreateURem(rem64, d.trip_count, ".omp.coord");
                if (rev > 1) {
                    rem64 = be_state->builder->CreateUDiv(rem64, d.trip_count, ".omp.rem");
                }
            }
            llvm::Value* delta64 = be_state->builder->CreateMul(coord64, d.step_mag, ".omp.delta");
            llvm::Value* cur_var_val = nullptr;
            if (d.var_ty->isPointerTy()) {
                llvm::Value* signed_off = d.is_descending ? be_state->builder->CreateNeg(delta64) : delta64;
                cur_var_val = be_state->builder->CreateGEP(
                    llvm::Type::getInt8Ty(Ctx), d.lb_val, signed_off, ".omp.ptr.iv");
            } else {
                llvm::Value* lb_c = be_state->builder->CreateIntCast(d.lb_val, d.var_ty, d.is_signed);
                llvm::Value* dl_c = be_state->builder->CreateIntCast(delta64, d.var_ty, false);
                cur_var_val = d.is_descending ? be_state->builder->CreateSub(lb_c, dl_c, ".omp.int.iv")
                                              : be_state->builder->CreateAdd(lb_c, dl_c, ".omp.int.iv");
            }
            be_state->builder->CreateStore(cur_var_val, d.var_slot);
        }

        size_t cleanup_depth = be_state->cleanup_stack.cleanups.size();
        be_state->continue_blocks.push_back(cont_target);
        be_state->continue_cleanup_depths.push_back(cleanup_depth);
        be_state->break_blocks.push_back(brk_target);
        be_state->break_cleanup_depths.push_back(cleanup_depth);
        llvm_gen_be_error_t b_err = llvm_lower_statement(innermost_body);
        be_state->continue_blocks.pop_back();
        be_state->continue_cleanup_depths.pop_back();
        be_state->break_blocks.pop_back();
        be_state->break_cleanup_depths.pop_back();
        return b_err;
    };

    llvm::BasicBlock* after_chunks_bb = llvm::BasicBlock::Create(Ctx, "omp.for.after_chunks", parent_fn);

    if (!is_dynamic_sched) {
        be_state->builder->CreateStore(zero_idx, plower);
        be_state->builder->CreateStore(max_ub, pupper);
        be_state->builder->CreateStore(one_idx, pstride);

        std::string init_fn_name = std::string("__kmpc_for_static_init_") + suffix;
        llvm::FunctionType* init_ty = llvm::FunctionType::get(
            void_ty, {ptr_ty, i32_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, idx_ty, idx_ty}, false);
        llvm::FunctionCallee init_call = M->getOrInsertFunction(init_fn_name, init_ty);
        be_state->builder->CreateCall(init_call, {
            ident, global_tid, llvm::ConstantInt::get(i32_ty, sched_kind),
            plastiter, plower, pupper, pstride, one_idx, chunk_idx
        });

        llvm::BasicBlock* chunk_cond_bb = llvm::BasicBlock::Create(Ctx, "omp.static.chunk.cond", parent_fn);
        llvm::BasicBlock* chunk_body_bb = llvm::BasicBlock::Create(Ctx, "omp.static.chunk.body", parent_fn);
        llvm::BasicBlock* chunk_step_bb = llvm::BasicBlock::Create(Ctx, "omp.static.chunk.step", parent_fn);
        be_state->builder->CreateBr(chunk_cond_bb);

        be_state->builder->SetInsertPoint(chunk_cond_bb);
        llvm::Value* cur_lb = be_state->builder->CreateLoad(idx_ty, plower, ".omp.cur.lb");
        llvm::Value* chunk_in_range = use_signed ? be_state->builder->CreateICmpSLE(cur_lb, max_ub)
                                                 : be_state->builder->CreateICmpULE(cur_lb, max_ub);
        be_state->builder->CreateCondBr(chunk_in_range, chunk_body_bb, after_chunks_bb);

        be_state->builder->SetInsertPoint(chunk_body_bb);
        llvm::Value* raw_chunk_ub = be_state->builder->CreateLoad(idx_ty, pupper, ".omp.raw.ub");
        llvm::Value* over_max = use_signed ? be_state->builder->CreateICmpSGT(raw_chunk_ub, max_ub)
                                           : be_state->builder->CreateICmpUGT(raw_chunk_ub, max_ub);
        llvm::Value* clamped_ub = be_state->builder->CreateSelect(over_max, max_ub, raw_chunk_ub, ".omp.clamped.ub");
        be_state->builder->CreateStore(cur_lb, iv_slot);

        llvm::BasicBlock* inner_cond_bb = llvm::BasicBlock::Create(Ctx, "omp.static.inner.cond", parent_fn);
        llvm::BasicBlock* inner_body_bb = llvm::BasicBlock::Create(Ctx, "omp.static.inner.body", parent_fn);
        llvm::BasicBlock* inner_inc_bb = llvm::BasicBlock::Create(Ctx, "omp.static.inner.inc", parent_fn);
        llvm::BasicBlock* inner_end_bb = has_explicit_chunk ? chunk_step_bb : after_chunks_bb;
        be_state->builder->CreateBr(inner_cond_bb);

        be_state->builder->SetInsertPoint(inner_cond_bb);
        llvm::Value* iv_v = be_state->builder->CreateLoad(idx_ty, iv_slot);
        llvm::Value* iv_le = use_signed ? be_state->builder->CreateICmpSLE(iv_v, clamped_ub)
                                        : be_state->builder->CreateICmpULE(iv_v, clamped_ub);
        be_state->builder->CreateCondBr(iv_le, inner_body_bb, inner_end_bb);

        be_state->builder->SetInsertPoint(inner_body_bb);
        llvm_gen_be_error_t b_err = emit_index_recovery_and_body(inner_inc_bb, after_chunks_bb);
        if (b_err != llvm_gen_be_error_t::ok) {
            LLVM_GEN_BE_CHECK(restore_standalone_reds(false));
            return b_err;
        }
        if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
            be_state->builder->CreateBr(inner_inc_bb);
        }

        be_state->builder->SetInsertPoint(inner_inc_bb);
        llvm::Value* next_iv = be_state->builder->CreateAdd(
            be_state->builder->CreateLoad(idx_ty, iv_slot), one_idx);
        be_state->builder->CreateStore(next_iv, iv_slot);
        be_state->builder->CreateBr(inner_cond_bb);

        be_state->builder->SetInsertPoint(chunk_step_bb);
        if (has_explicit_chunk) {
            llvm::Value* st_v = be_state->builder->CreateLoad(idx_ty, pstride);
            be_state->builder->CreateStore(
                be_state->builder->CreateAdd(be_state->builder->CreateLoad(idx_ty, plower), st_v), plower);
            be_state->builder->CreateStore(
                be_state->builder->CreateAdd(be_state->builder->CreateLoad(idx_ty, pupper), st_v), pupper);
            be_state->builder->CreateBr(chunk_cond_bb);
        } else {
            chunk_step_bb->eraseFromParent();
        }
    } else {
        std::string dinit_name = std::string("__kmpc_dispatch_init_") + suffix;
        std::string dnext_name = std::string("__kmpc_dispatch_next_") + suffix;
        llvm::FunctionType* dinit_ty = llvm::FunctionType::get(
            void_ty, {ptr_ty, i32_ty, i32_ty, idx_ty, idx_ty, idx_ty, idx_ty}, false);
        llvm::FunctionType* dnext_ty = llvm::FunctionType::get(
            i32_ty, {ptr_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty}, false);
        llvm::FunctionCallee dinit_call = M->getOrInsertFunction(dinit_name, dinit_ty);
        llvm::FunctionCallee dnext_call = M->getOrInsertFunction(dnext_name, dnext_ty);

        be_state->builder->CreateCall(dinit_call, {
            ident, global_tid, llvm::ConstantInt::get(i32_ty, sched_kind),
            zero_idx, max_ub, one_idx, chunk_idx
        });

        llvm::BasicBlock* disp_cond_bb = llvm::BasicBlock::Create(Ctx, "omp.disp.cond", parent_fn);
        llvm::BasicBlock* disp_body_bb = llvm::BasicBlock::Create(Ctx, "omp.disp.body", parent_fn);
        be_state->builder->CreateBr(disp_cond_bb);

        be_state->builder->SetInsertPoint(disp_cond_bb);
        llvm::Value* has_chunk = be_state->builder->CreateCall(
            dnext_call, {ident, global_tid, plastiter, plower, pupper, pstride});
        llvm::Value* nz = be_state->builder->CreateICmpNE(has_chunk, llvm::ConstantInt::get(i32_ty, 0));
        be_state->builder->CreateCondBr(nz, disp_body_bb, after_chunks_bb);

        be_state->builder->SetInsertPoint(disp_body_bb);
        llvm::Value* d_lb = be_state->builder->CreateLoad(idx_ty, plower);
        llvm::Value* d_ub = be_state->builder->CreateLoad(idx_ty, pupper);
        be_state->builder->CreateStore(d_lb, iv_slot);

        llvm::BasicBlock* inner_cond_bb = llvm::BasicBlock::Create(Ctx, "omp.disp.inner.cond", parent_fn);
        llvm::BasicBlock* inner_body_bb = llvm::BasicBlock::Create(Ctx, "omp.disp.inner.body", parent_fn);
        llvm::BasicBlock* inner_inc_bb = llvm::BasicBlock::Create(Ctx, "omp.disp.inner.inc", parent_fn);
        be_state->builder->CreateBr(inner_cond_bb);

        be_state->builder->SetInsertPoint(inner_cond_bb);
        llvm::Value* iv_v = be_state->builder->CreateLoad(idx_ty, iv_slot);
        llvm::Value* iv_le = use_signed ? be_state->builder->CreateICmpSLE(iv_v, d_ub)
                                        : be_state->builder->CreateICmpULE(iv_v, d_ub);
        be_state->builder->CreateCondBr(iv_le, inner_body_bb, disp_cond_bb);

        be_state->builder->SetInsertPoint(inner_body_bb);
        llvm_gen_be_error_t b_err = emit_index_recovery_and_body(inner_inc_bb, after_chunks_bb);
        if (b_err != llvm_gen_be_error_t::ok) {
            LLVM_GEN_BE_CHECK(restore_standalone_reds(false));
            return b_err;
        }
        if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
            be_state->builder->CreateBr(inner_inc_bb);
        }

        be_state->builder->SetInsertPoint(inner_inc_bb);
        llvm::Value* next_iv = be_state->builder->CreateAdd(
            be_state->builder->CreateLoad(idx_ty, iv_slot), one_idx);
        be_state->builder->CreateStore(next_iv, iv_slot);
        be_state->builder->CreateBr(inner_cond_bb);
    }

    be_state->builder->SetInsertPoint(after_chunks_bb);
    if (dir_str.find("lastprivate(") != std::string::npos) {
        llvm::BasicBlock* last_bb = llvm::BasicBlock::Create(Ctx, "omp.lastprivate.then", parent_fn);
        llvm::BasicBlock* after_last_bb = llvm::BasicBlock::Create(Ctx, "omp.lastprivate.end", parent_fn);
        llvm::Value* is_last_val = be_state->builder->CreateLoad(i32_ty, plastiter, ".omp.is_last.val");
        llvm::Value* is_last_cmp = be_state->builder->CreateICmpNE(
            is_last_val, llvm::ConstantInt::get(i32_ty, 0), ".omp.is_last.cond");
        be_state->builder->CreateCondBr(is_last_cmp, last_bb, after_last_bb);

        be_state->builder->SetInsertPoint(last_bb);
        for (const OmpCollapsedLoopDim& d : dims) {
            const char* iv_name = d.ind_var->source_corresp.name ? d.ind_var->source_corresp.name : "";
            bool iv_in_last = false;
            LLVM_GEN_BE_CHECK(is_var_in_omp_clause(directive, "lastprivate", iv_name, &iv_in_last));
            if (iv_in_last) {
                llvm::Value* final_delta = be_state->builder->CreateMul(d.trip_count, d.step_mag);
                llvm::Value* next_past_last = nullptr;
                if (d.var_ty->isPointerTy()) {
                    llvm::Value* soff = d.is_descending ? be_state->builder->CreateNeg(final_delta) : final_delta;
                    next_past_last = be_state->builder->CreateGEP(llvm::Type::getInt8Ty(Ctx), d.lb_val, soff);
                } else {
                    llvm::Value* lb_c = be_state->builder->CreateIntCast(d.lb_val, d.var_ty, d.is_signed);
                    llvm::Value* dl_c = be_state->builder->CreateIntCast(final_delta, d.var_ty, false);
                    next_past_last = d.is_descending ? be_state->builder->CreateSub(lb_c, dl_c)
                                                     : be_state->builder->CreateAdd(lb_c, dl_c);
                }
                be_state->builder->CreateStore(next_past_last, d.var_slot);
            }
        }
        be_state->builder->CreateBr(after_last_bb);
        be_state->builder->SetInsertPoint(after_last_bb);
    }

    if (!is_dynamic_sched) {
        llvm::FunctionType* fini_ty = llvm::FunctionType::get(void_ty, {ptr_ty, i32_ty}, false);
        llvm::FunctionCallee fini_call = M->getOrInsertFunction("__kmpc_for_static_fini", fini_ty);
        be_state->builder->CreateCall(fini_call, {ident, global_tid});
    }
    be_state->builder->CreateBr(done_bb);

    be_state->builder->SetInsertPoint(done_bb);
    if (dir_str.find("nowait") == std::string::npos && dir_str.find("omp parallel") == std::string::npos) {
        LLVM_GEN_BE_CHECK(lower_omp_barrier(stmt));
    }
    return restore_standalone_reds(true);
}

/**
 * @brief Lowers an OpenMP critical section into LLVM IR.
 * @details Emits a `[8 x i32]` global critical lock (`gomp_critical_user_.var`), calls
 *          `__kmpc_critical`, lowers the critical region body statement, and calls `__kmpc_end_critical`.
 * @param[in] stmt The EDG statement representing the critical section.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_critical(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    
    llvm::FunctionType* crit_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {ptr_ty, i32_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee crit_call = M->getOrInsertFunction("__kmpc_critical", crit_ty);
    llvm::FunctionCallee end_crit_call = M->getOrInsertFunction("__kmpc_end_critical", crit_ty);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));

    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));
    llvm::ArrayType* lock_arr_ty = llvm::ArrayType::get(i32_ty, 8);
    llvm::GlobalVariable* lock_gv = M->getGlobalVariable("gomp_critical_user_.var", true);
    if (!lock_gv) {
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(M, lock_arr_ty, false, llvm::GlobalValue::CommonLinkage, llvm::ConstantAggregateZero::get(lock_arr_ty), "gomp_critical_user_.var", &lock_gv));
    }
    
    be_state->builder->CreateCall(crit_call, {ident, global_tid, lock_gv});
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    be_state->builder->CreateCall(end_crit_call, {ident, global_tid, lock_gv});
    
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP barrier synchronization into LLVM IR.
 * @details Emits a call to `__kmpc_barrier` with the directive's `ident_t` descriptor.
 * @param[in] stmt The EDG statement representing the barrier.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_barrier(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* barr_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee barr_call = M->getOrInsertFunction("__kmpc_barrier", barr_ty);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));
    
    be_state->builder->CreateCall(barr_call, {ident, global_tid});
    return llvm_gen_be_error_t::ok;
}

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
    std::vector<omp_map_clause_entry_t>* out_entries) noexcept {
    if (!directive_text || !out_entries) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    out_entries->clear();
    std::string text = std::string(directive_text);

    size_t pos = 0;
    while (pos < text.size()) {
        size_t m_pos = text.find("map(", pos);
        size_t t_pos = text.find(" to(", pos);
        size_t f_pos = text.find(" from(", pos);

        size_t next_pos = std::string::npos;
        uint64_t default_flags = 0x23ULL; // TO | FROM | TARGET_PARAM
        size_t prefix_len = 4;
        if (m_pos != std::string::npos) {
            next_pos = m_pos;
            prefix_len = 4;
        }
        if (t_pos != std::string::npos && t_pos < next_pos) {
            next_pos = t_pos + 1;
            prefix_len = 3;
            default_flags = 0x21ULL; // TO | TARGET_PARAM
        }
        if (f_pos != std::string::npos && f_pos < next_pos) {
            next_pos = f_pos + 1;
            prefix_len = 5;
            default_flags = 0x22ULL; // FROM | TARGET_PARAM
        }
        if (next_pos == std::string::npos) break;

        size_t close_pos = text.find(')', next_pos + prefix_len);
        if (close_pos == std::string::npos) break;

        std::string inner = text.substr(next_pos + prefix_len, close_pos - (next_pos + prefix_len));
        pos = close_pos + 1;

        uint64_t flags = default_flags;
        size_t colon_pos = inner.find(':');
        size_t bracket_pos = inner.find('[');
        if (bracket_pos != std::string::npos && colon_pos > bracket_pos) {
            colon_pos = std::string::npos;
        }

        std::string vars_part = inner;
        if (colon_pos != std::string::npos) {
            std::string mod_part = inner.substr(0, colon_pos);
            vars_part = inner.substr(colon_pos + 1);
            bool has_always = (mod_part.find("always") != std::string::npos);
            if (mod_part.find("tofrom") != std::string::npos) {
                flags = 0x23ULL;
            } else if (mod_part.find("to") != std::string::npos) {
                flags = 0x21ULL;
            } else if (mod_part.find("from") != std::string::npos) {
                flags = 0x22ULL;
            } else if (mod_part.find("delete") != std::string::npos) {
                flags = 0x28ULL; // DELETE (0x8) | TARGET_PARAM (0x20)
            } else {
                flags = 0x20ULL; // alloc / release
            }
            if (has_always) {
                flags |= 0x4ULL; // OMP_TGT_MAPTYPE_ALWAYS
            }
        }

        size_t item_start = 0;
        while (item_start < vars_part.size()) {
            size_t comma = vars_part.find(',', item_start);
            std::string raw_item = (comma == std::string::npos)
                                       ? vars_part.substr(item_start)
                                       : vars_part.substr(item_start, comma - item_start);
            item_start = (comma == std::string::npos) ? vars_part.size() : comma + 1;

            size_t first = raw_item.find_first_not_of(" \t");
            if (first == std::string::npos) continue;
            size_t last = raw_item.find_last_not_of(" \t");
            std::string item = raw_item.substr(first, last - first + 1);

            omp_map_clause_entry_t entry = {};
            entry.expr_text = item;
            entry.map_type_flags = flags;
            entry.section_offset_elems = 0;
            entry.section_length_elems = 1;
            entry.is_array_section = false;
            entry.is_struct_member = false;

            size_t brk_open = item.find('[');
            size_t brk_close = item.find(']');
            std::string head = item;
            if (brk_open != std::string::npos && brk_close != std::string::npos) {
                entry.is_array_section = true;
                entry.map_type_flags |= 0x10ULL; // OMP_TGT_MAPTYPE_PTR_AND_OBJ
                head = item.substr(0, brk_open);
                std::string slice = item.substr(brk_open + 1, brk_close - brk_open - 1);
                size_t slice_colon = slice.find(':');
                if (slice_colon != std::string::npos) {
                    std::string lo_s = slice.substr(0, slice_colon);
                    std::string len_s = slice.substr(slice_colon + 1);
                    entry.section_offset_elems = static_cast<uint64_t>(std::strtoull(lo_s.c_str(), nullptr, 10));
                    uint64_t parsed_len = static_cast<uint64_t>(std::strtoull(len_s.c_str(), nullptr, 10));
                    if (parsed_len > 0) {
                        entry.section_length_elems = parsed_len;
                    }
                }
            }

            size_t dot_pos = head.find('.');
            if (dot_pos != std::string::npos) {
                entry.is_struct_member = true;
                entry.base_var_name = head.substr(0, dot_pos);
                entry.map_type_flags |= 0x1000000000000ULL; // OMP_TGT_MAPTYPE_MEMBER_OF(1)
            } else {
                entry.base_var_name = head;
            }
            out_entries->push_back(entry);
        }
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Builds `.offload_baseptrs`, `.offload_ptrs`, `.offload_sizes`, and `.offload_maptypes` stack arrays.
 * @details Resolves each `omp_map_clause_entry_t` against `be_state->local_vars` (or creates a default
 *          1-entry descriptor when @p entries is empty) and stores base pointers, element/member pointers,
 *          byte sizes, and `OMP_TGT_MAPTYPE_*` bitmasks.
 * @param[in] entries Parsed map clause descriptors.
 * @param[out] out_base_ptrs Receives the `.offload_baseptrs` alloca pointer.
 * @param[out] out_ptrs Receives the `.offload_ptrs` alloca pointer.
 * @param[out] out_sizes Receives the `.offload_sizes` alloca pointer.
 * @param[out] out_map_types Receives the `.offload_maptypes` alloca pointer.
 * @param[out] out_arg_num Receives the `i32` argument count `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t emit_omp_mapper_arrays(
    const std::vector<omp_map_clause_entry_t>& entries,
    llvm::AllocaInst** out_base_ptrs,
    llvm::AllocaInst** out_ptrs,
    llvm::AllocaInst** out_sizes,
    llvm::AllocaInst** out_map_types,
    llvm::Value** out_arg_num) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(Ctx);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    uint32_t count = entries.empty() ? 1u : static_cast<uint32_t>(entries.size());
    llvm::ArrayType* ptr_arr_ty = llvm::ArrayType::get(ptr_ty, count);
    llvm::ArrayType* i64_arr_ty = llvm::ArrayType::get(i64_ty, count);

    llvm::AllocaInst* base_ptrs = be_state->builder->CreateAlloca(ptr_arr_ty, nullptr, ".offload_baseptrs");
    llvm::AllocaInst* ptrs = be_state->builder->CreateAlloca(ptr_arr_ty, nullptr, ".offload_ptrs");
    llvm::AllocaInst* sizes = be_state->builder->CreateAlloca(i64_arr_ty, nullptr, ".offload_sizes");
    llvm::AllocaInst* map_types = be_state->builder->CreateAlloca(i64_arr_ty, nullptr, ".offload_maptypes");
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    llvm::Value* zero_idx = llvm::ConstantInt::get(i32_ty, 0);

    const llvm::DataLayout& dl = be_state->module->getDataLayout();
    if (entries.empty()) {
        be_state->builder->CreateStore(null_ptr, base_ptrs);
        be_state->builder->CreateStore(null_ptr, ptrs);
        be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, dl.getPointerSize()), sizes);
        be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, 0x23ULL), map_types);
    } else {
        for (uint32_t idx = 0; idx < count; ++idx) {
            const omp_map_clause_entry_t& e = entries[idx];
            llvm::Value* base_val = null_ptr;
            a_variable_ptr matched_var = nullptr;
            for (const auto& kv : be_state->local_vars) {
                if (kv.first && kv.second && kv.first->source_corresp.name &&
                    e.base_var_name == kv.first->source_corresp.name) {
                    matched_var = kv.first;
                    base_val = kv.second;
                    break;
                }
            }
            if (base_val == null_ptr) {
                if (llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(e.base_var_name)) {
                    base_val = gv;
                }
            }

            uint64_t elem_bytes = 0;
            uint64_t byte_size = 0;
            uint64_t member_offset = 0;
            llvm::Value* ptr_base = base_val;

            if (matched_var && matched_var->type) {
                llvm::Type* var_llvm_ty = nullptr;
                LLVM_GEN_BE_CHECK(get_llvm_type(matched_var->type, &var_llvm_ty));
                a_type_ptr ut = nullptr;
                LLVM_GEN_BE_CHECK(unwrap_omp_typerefs(matched_var->type, &ut));
                if (e.is_array_section) {
                    elem_bytes = dl.getTypeAllocSize(var_llvm_ty);
                    if (ut && ut->kind == tk_pointer) {
                        if (base_val != null_ptr) {
                            ptr_base = be_state->builder->CreateLoad(ptr_ty, base_val, ".offload.ptr.load");
                        }
                        a_type_ptr pt_elem = nullptr;
                        LLVM_GEN_BE_CHECK(unwrap_omp_typerefs(ut->variant.pointer.type, &pt_elem));
                        if (pt_elem && pt_elem->kind != tk_void) {
                            llvm::Type* pt_llvm_ty = nullptr;
                            LLVM_GEN_BE_CHECK(get_llvm_type(pt_elem, &pt_llvm_ty));
                            elem_bytes = dl.getTypeAllocSize(pt_llvm_ty);
                        }
                    } else if (ut && ut->kind == tk_array && ut->variant.array.element_type) {
                        llvm::Type* arr_elem_ty = nullptr;
                        LLVM_GEN_BE_CHECK(get_llvm_type(ut->variant.array.element_type, &arr_elem_ty));
                        elem_bytes = dl.getTypeAllocSize(arr_elem_ty);
                    } else if (llvm::ArrayType* at = llvm::dyn_cast<llvm::ArrayType>(var_llvm_ty)) {
                        elem_bytes = dl.getTypeAllocSize(at->getElementType());
                    }
                    byte_size = e.section_length_elems * elem_bytes;
                } else if (e.is_struct_member && ut &&
                           (ut->kind == tk_struct || ut->kind == tk_class || ut->kind == tk_union)) {
                    byte_size = dl.getTypeAllocSize(var_llvm_ty);
                    size_t dot = e.expr_text.find('.');
                    std::string field_name = e.expr_text.substr(dot + 1);
                    size_t brk = field_name.find('[');
                    if (brk != std::string::npos) field_name = field_name.substr(0, brk);
                    for (a_field_ptr f = ut->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
                        if (f->source_corresp.name && field_name == f->source_corresp.name) {
                            member_offset = static_cast<uint64_t>(f->offset);
                            if (f->type) {
                                llvm::Type* f_ty = nullptr;
                                LLVM_GEN_BE_CHECK(get_llvm_type(f->type, &f_ty));
                                byte_size = dl.getTypeAllocSize(f_ty);
                            }
                            break;
                        }
                    }
                } else {
                    byte_size = dl.getTypeAllocSize(var_llvm_ty);
                }
            } else if (llvm::AllocaInst* ai = llvm::dyn_cast<llvm::AllocaInst>(base_val)) {
                llvm::Type* alloc_ty = ai->getAllocatedType();
                if (e.is_array_section) {
                    if (llvm::ArrayType* at = llvm::dyn_cast<llvm::ArrayType>(alloc_ty)) {
                        elem_bytes = dl.getTypeAllocSize(at->getElementType());
                    } else {
                        elem_bytes = dl.getTypeAllocSize(alloc_ty);
                    }
                    byte_size = e.section_length_elems * elem_bytes;
                } else {
                    byte_size = dl.getTypeAllocSize(alloc_ty);
                }
            } else if (llvm::GlobalVariable* gv = llvm::dyn_cast<llvm::GlobalVariable>(base_val)) {
                llvm::Type* gv_ty = gv->getValueType();
                elem_bytes = dl.getTypeAllocSize(gv_ty);
                byte_size = e.is_array_section ? (e.section_length_elems * elem_bytes) : elem_bytes;
            } else {
                elem_bytes = dl.getTypeAllocSize(i32_ty);
                byte_size = e.is_array_section ? (e.section_length_elems * elem_bytes) : dl.getPointerSize();
            }

            llvm::Value* ptr_val = ptr_base;
            uint64_t total_byte_off = member_offset + (e.section_offset_elems * (elem_bytes > 0 ? elem_bytes : 1ULL));
            if (total_byte_off > 0 && ptr_base != null_ptr) {
                ptr_val = be_state->builder->CreateGEP(
                    i8_ty, ptr_base, llvm::ConstantInt::get(i64_ty, total_byte_off), ".offload.sec.ptr");
            }
            llvm::Value* i_val = llvm::ConstantInt::get(i32_ty, idx);
            llvm::Value* bp_slot = be_state->builder->CreateInBoundsGEP(ptr_arr_ty, base_ptrs, {zero_idx, i_val});
            llvm::Value* p_slot = be_state->builder->CreateInBoundsGEP(ptr_arr_ty, ptrs, {zero_idx, i_val});
            llvm::Value* s_slot = be_state->builder->CreateInBoundsGEP(i64_arr_ty, sizes, {zero_idx, i_val});
            llvm::Value* m_slot = be_state->builder->CreateInBoundsGEP(i64_arr_ty, map_types, {zero_idx, i_val});
            be_state->builder->CreateStore(base_val, bp_slot);
            be_state->builder->CreateStore(ptr_val, p_slot);
            be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, byte_size), s_slot);
            be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, e.map_type_flags), m_slot);
        }
    }

    *out_base_ptrs = base_ptrs;
    *out_ptrs = ptrs;
    *out_sizes = sizes;
    *out_map_types = map_types;
    *out_arg_num = llvm::ConstantInt::get(i32_ty, count);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `target data` scoped data mapping construct into LLVM IR.
 * @details Emits `__tgt_target_data_begin_mapper` before lowering the structured body statement
 *          and `__tgt_target_data_end_mapper` after the body completes.
 * @param[in] stmt The EDG statement representing the `target data` body.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_data(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    const char* directive = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_directive_text(stmt, &directive));
    std::vector<omp_map_clause_entry_t> entries;
    LLVM_GEN_BE_CHECK(parse_omp_map_clauses(directive ? directive : "", &entries));

    llvm::AllocaInst* base_ptrs = nullptr;
    llvm::AllocaInst* ptrs = nullptr;
    llvm::AllocaInst* sizes = nullptr;
    llvm::AllocaInst* map_types = nullptr;
    llvm::Value* arg_num = nullptr;
    LLVM_GEN_BE_CHECK(
        emit_omp_mapper_arrays(entries, &base_ptrs, &ptrs, &sizes, &map_types, &arg_num));

    llvm::FunctionType* map_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {i64_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty},
        false);
    llvm::FunctionCallee map_begin = M->getOrInsertFunction("__tgt_target_data_begin_mapper", map_ty);
    llvm::FunctionCallee map_end = M->getOrInsertFunction("__tgt_target_data_end_mapper", map_ty);

    llvm::Constant* device_id = llvm::ConstantInt::getSigned(i64_ty, -1);
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    be_state->builder->CreateCall(map_begin, {
        device_id, arg_num, base_ptrs, ptrs, sizes, map_types, null_ptr, null_ptr
    });
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    be_state->builder->CreateCall(map_end, {
        device_id, arg_num, base_ptrs, ptrs, sizes, map_types, null_ptr, null_ptr
    });
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `target enter data` standalone directive into LLVM IR.
 * @details Populates mapper arrays (`base_ptrs`, `ptrs`, `sizes`, `map_types`) from parsed
 *          `map(to:/alloc:)` clauses and emits `__tgt_target_data_begin_mapper`.
 * @param[in] stmt The EDG statement associated with `omp target enter data`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_enter_data(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    const char* directive = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_directive_text(stmt, &directive));
    std::vector<omp_map_clause_entry_t> entries;
    LLVM_GEN_BE_CHECK(parse_omp_map_clauses(directive ? directive : "", &entries));

    llvm::AllocaInst* base_ptrs = nullptr;
    llvm::AllocaInst* ptrs = nullptr;
    llvm::AllocaInst* sizes = nullptr;
    llvm::AllocaInst* map_types = nullptr;
    llvm::Value* arg_num = nullptr;
    LLVM_GEN_BE_CHECK(
        emit_omp_mapper_arrays(entries, &base_ptrs, &ptrs, &sizes, &map_types, &arg_num));

    llvm::FunctionType* map_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {i64_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty},
        false);
    llvm::FunctionCallee map_begin = M->getOrInsertFunction("__tgt_target_data_begin_mapper", map_ty);
    llvm::Constant* device_id = llvm::ConstantInt::getSigned(i64_ty, -1);
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    be_state->builder->CreateCall(map_begin, {
        device_id, arg_num, base_ptrs, ptrs, sizes, map_types, null_ptr, null_ptr
    });
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `target exit data` standalone directive into LLVM IR.
 * @details Populates mapper arrays from parsed `map(from:/release:/delete:)` clauses and
 *          emits `__tgt_target_data_end_mapper`.
 * @param[in] stmt The EDG statement associated with `omp target exit data`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_exit_data(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    const char* directive = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_directive_text(stmt, &directive));
    std::vector<omp_map_clause_entry_t> entries;
    LLVM_GEN_BE_CHECK(parse_omp_map_clauses(directive ? directive : "", &entries));

    llvm::AllocaInst* base_ptrs = nullptr;
    llvm::AllocaInst* ptrs = nullptr;
    llvm::AllocaInst* sizes = nullptr;
    llvm::AllocaInst* map_types = nullptr;
    llvm::Value* arg_num = nullptr;
    LLVM_GEN_BE_CHECK(
        emit_omp_mapper_arrays(entries, &base_ptrs, &ptrs, &sizes, &map_types, &arg_num));

    llvm::FunctionType* map_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {i64_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty},
        false);
    llvm::FunctionCallee map_end = M->getOrInsertFunction("__tgt_target_data_end_mapper", map_ty);
    llvm::Constant* device_id = llvm::ConstantInt::getSigned(i64_ty, -1);
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    be_state->builder->CreateCall(map_end, {
        device_id, arg_num, base_ptrs, ptrs, sizes, map_types, null_ptr, null_ptr
    });
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `target update` standalone directive into LLVM IR.
 * @details Populates mapper arrays from `to(...)` / `from(...)` clauses and emits
 *          `__tgt_target_data_update_mapper`.
 * @param[in] stmt The EDG statement associated with `omp target update`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_update(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    const char* directive = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_directive_text(stmt, &directive));
    std::vector<omp_map_clause_entry_t> entries;
    LLVM_GEN_BE_CHECK(parse_omp_map_clauses(directive ? directive : "", &entries));

    llvm::AllocaInst* base_ptrs = nullptr;
    llvm::AllocaInst* ptrs = nullptr;
    llvm::AllocaInst* sizes = nullptr;
    llvm::AllocaInst* map_types = nullptr;
    llvm::Value* arg_num = nullptr;
    LLVM_GEN_BE_CHECK(
        emit_omp_mapper_arrays(entries, &base_ptrs, &ptrs, &sizes, &map_types, &arg_num));

    llvm::FunctionType* upd_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {i64_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty},
        false);
    llvm::FunctionCallee upd_call = M->getOrInsertFunction("__tgt_target_data_update_mapper", upd_ty);
    llvm::Value* null_p = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    be_state->builder->CreateCall(upd_call, {
        llvm::ConstantInt::getSigned(i64_ty, -1), arg_num,
        base_ptrs, ptrs, sizes, map_types, null_p, null_p
    });
    return llvm_gen_be_error_t::ok;
}

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
    llvm::SmallVectorImpl<char>* out_device_image) noexcept {
    if (!host_mod || !out_device_image) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    bool is_nvptx = llvm::StringRef(offload_triple).starts_with("nvptx");
    bool is_amdgpu = llvm::StringRef(offload_triple).starts_with("amdgcn");
    if (!is_nvptx && !is_amdgpu) {
        return llvm_gen_be_error_t::gpu_offload_failed;
    }
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());

    out_device_image->clear();
    llvm::LLVMContext& Ctx = host_mod->getContext();
    llvm::Module dev_mod{"omp_device_module", Ctx};
    EDG_LLVM_SET_TARGET_TRIPLE(&dev_mod, offload_triple);

    std::string target_lookup_err;
#if LLVM_VERSION_MAJOR >= 21
    const llvm::Target* registered_dev_target =
        llvm::TargetRegistry::lookupTarget(llvm::Triple(offload_triple), target_lookup_err);
#else
    const llvm::Target* registered_dev_target =
        llvm::TargetRegistry::lookupTarget(offload_triple, target_lookup_err);
#endif
    if (registered_dev_target != nullptr) {
        dev_mod.setDataLayout(host_mod->getDataLayout());
    }

    llvm::ValueToValueMapTy vmap;
    for (const llvm::GlobalVariable& gv : host_mod->globals()) {
        if (gv.getSection() == ".omp_offloading.entries" ||
            gv.getSection() == ".llvm.offloading" ||
            gv.getSection() == ".omp_offloading.device_image") {
            continue;
        }
        llvm::GlobalVariable* dev_gv = nullptr;
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(
            &dev_mod, gv.getValueType(), gv.isConstant(),
            llvm::GlobalValue::ExternalLinkage, nullptr, gv.getName(), &dev_gv));
        vmap[&gv] = dev_gv;
    }
    for (const llvm::Function& decl_fn : *host_mod) {
        llvm::Function* dev_decl = llvm::Function::Create(
            decl_fn.getFunctionType(), llvm::GlobalValue::ExternalLinkage,
            decl_fn.getName(), &dev_mod);
        vmap[&decl_fn] = dev_decl;
    }

    llvm::NamedMDNode* nvvm_annot = is_amdgpu ? nullptr : dev_mod.getOrInsertNamedMetadata("nvvm.annotations");
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);

    for (const llvm::Function& fn : *host_mod) {
        bool is_kernel = fn.getName().starts_with("__omp_offloading_");
        bool is_declare_target = fn.hasFnAttribute("omp_declare_target");
        if (!is_kernel && !is_declare_target) continue;

        llvm::Function* dev_fn = llvm::cast<llvm::Function>(vmap[&fn]);
        if (!offload_arch.empty()) {
            dev_fn->addFnAttr("target-cpu", offload_arch);
        }
        if (is_kernel) {
            dev_fn->setCallingConv(is_amdgpu ? llvm::CallingConv::AMDGPU_KERNEL : llvm::CallingConv::PTX_Kernel);
            if (is_amdgpu) {
                dev_fn->addFnAttr("amdgpu-flat-work-group-size", "1,256");
            } else {
                llvm::Metadata* md_vals[] = {
                    llvm::ValueAsMetadata::get(dev_fn),
                    llvm::MDString::get(Ctx, "kernel"),
                    llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(i32_ty, 1))
                };
                nvvm_annot->addOperand(llvm::MDNode::get(Ctx, md_vals));
            }
        }

        if (!fn.isDeclaration()) {
            llvm::Function::arg_iterator dest_arg = dev_fn->arg_begin();
            for (const llvm::Argument& src_arg : fn.args()) {
                dest_arg->setName(src_arg.getName());
                vmap[&src_arg] = &*dest_arg++;
            }
            llvm::SmallVector<llvm::ReturnInst*, 4> returns;
            llvm::CloneFunctionInto(
                dev_fn, &fn, vmap, llvm::CloneFunctionChangeType::DifferentModule, returns);
            if (is_kernel) {
                llvm::FunctionType* kinit_ty = llvm::FunctionType::get(
                    i32_ty, {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt8Ty(Ctx), llvm::Type::getInt1Ty(Ctx)}, false);
                llvm::FunctionType* kdeinit_ty = llvm::FunctionType::get(
                    void_ty, {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt8Ty(Ctx)}, false);
                llvm::FunctionCallee kinit = dev_mod.getOrInsertFunction("__kmpc_target_init", kinit_ty);
                llvm::FunctionCallee kdeinit = dev_mod.getOrInsertFunction("__kmpc_target_deinit", kdeinit_ty);
                llvm::Value* null_ident = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));

                llvm::IRBuilder<> entry_builder(&dev_fn->getEntryBlock(), dev_fn->getEntryBlock().getFirstInsertionPt());
                entry_builder.CreateCall(
                    kinit, {null_ident, llvm::ConstantInt::get(llvm::Type::getInt8Ty(Ctx), 1), entry_builder.getTrue()});
                for (llvm::ReturnInst* ret_inst : returns) {
                    llvm::IRBuilder<> ret_builder(ret_inst);
                    ret_builder.CreateCall(kdeinit, {null_ident, llvm::ConstantInt::get(llvm::Type::getInt8Ty(Ctx), 1)});
                }
            }
        } else if (is_kernel) {
            llvm::BasicBlock* dev_bb = llvm::BasicBlock::Create(Ctx, "entry", dev_fn);
            llvm::IRBuilder<> dev_builder(dev_bb);
            llvm::FunctionType* kinit_ty = llvm::FunctionType::get(
                i32_ty, {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt8Ty(Ctx), llvm::Type::getInt1Ty(Ctx)}, false);
            llvm::FunctionType* kdeinit_ty = llvm::FunctionType::get(
                void_ty, {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt8Ty(Ctx)}, false);
            llvm::FunctionCallee kinit = dev_mod.getOrInsertFunction("__kmpc_target_init", kinit_ty);
            llvm::FunctionCallee kdeinit = dev_mod.getOrInsertFunction("__kmpc_target_deinit", kdeinit_ty);
            llvm::Value* null_ident = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));
            dev_builder.CreateCall(kinit, {null_ident, llvm::ConstantInt::get(llvm::Type::getInt8Ty(Ctx), 1), dev_builder.getTrue()});
            dev_builder.CreateCall(kdeinit, {null_ident, llvm::ConstantInt::get(llvm::Type::getInt8Ty(Ctx), 1)});
            dev_builder.CreateRetVoid();
        }
    }

    llvm::raw_svector_ostream bc_os{*out_device_image};
    llvm::WriteBitcodeToFile(dev_mod, bc_os);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP target region for device offloading into LLVM IR.
 * @details Outlines the device kernel entry routine `__omp_offloading_<line>_<func>`, emits the
 *          `.omp_offloading.entries` global metadata table (`__tgt_offload_entry`), parses
 *          `map(...)` clauses (including array sections and struct members) via `parse_omp_map_clauses`,
 *          handles `nowait` and `depend(...)` clauses via `__tgt_target_kernel_nowait` and
 *          `__kmpc_omp_task_with_deps`, and embeds the fatbinary descriptor via
 *          `llvm_emit_omp_device_offload_module`.
 * @param[in] stmt The EDG statement representing the target region.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t lower_omp_target_region(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(Ctx);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    const char* directive = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_directive_text(stmt, &directive));
    std::string dir_str = directive ? directive : "";

    std::vector<omp_map_clause_entry_t> entries;
    LLVM_GEN_BE_CHECK(parse_omp_map_clauses(dir_str.c_str(), &entries));

    llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
    std::string kernel_name = "__omp_offloading_" + std::to_string(stmt->position.seq) + "_" + parent_fn->getName().str();

    bool is_nowait = (dir_str.find("nowait") != std::string::npos);
    bool has_depend = (dir_str.find("depend(") != std::string::npos);

    llvm::FunctionType* tgt_ty = llvm::FunctionType::get(
        i32_ty,
        {i64_ty, ptr_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty},
        false
    );
    const char* kernel_rt_name = is_nowait ? "__tgt_target_kernel_nowait" : "__tgt_target_kernel";
    llvm::FunctionCallee tgt_call = M->getOrInsertFunction(kernel_rt_name, tgt_ty);
    
    llvm::FunctionType* map_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {i64_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee map_begin = M->getOrInsertFunction("__tgt_target_data_begin_mapper", map_ty);
    llvm::FunctionCallee map_end = M->getOrInsertFunction("__tgt_target_data_end_mapper", map_ty);

    llvm::FunctionType* kernel_fn_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {}, false);
    llvm::Function* kernel_fn = llvm::Function::Create(
        kernel_fn_ty, llvm::GlobalValue::InternalLinkage, kernel_name, M);
    kernel_fn->addFnAttr("omp_target_kernel");
    llvm::BasicBlock* kernel_bb = llvm::BasicBlock::Create(Ctx, "entry", kernel_fn);
    llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
    be_state->builder->SetInsertPoint(kernel_bb);
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) {
        be_state->builder->restoreIP(saved_ip);
        kernel_fn->eraseFromParent();
        return err;
    }
    if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
        be_state->builder->CreateRetVoid();
    }
    be_state->builder->restoreIP(saved_ip);

    // Emit host offload ID and .omp_offloading.entries table entry (__tgt_offload_entry)
    llvm::GlobalVariable* host_entry_id = nullptr;
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(M, i8_ty, true, llvm::GlobalValue::InternalLinkage, llvm::ConstantInt::get(i8_ty, 0), kernel_name + ".region_id", &host_entry_id));
    llvm::Constant* entry_name_const = llvm::ConstantDataArray::getString(Ctx, kernel_name, true);
    llvm::GlobalVariable* entry_name_gv = nullptr;
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(M, entry_name_const->getType(), true, llvm::GlobalValue::InternalLinkage, entry_name_const, ".omp_offloading.entry_name", &entry_name_gv));
    llvm::StructType* offload_entry_ty = llvm::StructType::get(Ctx, {ptr_ty, ptr_ty, i64_ty, i32_ty, i32_ty});
    llvm::Constant* offload_entry_init = llvm::ConstantStruct::get(
        offload_entry_ty,
        {host_entry_id, entry_name_gv, llvm::ConstantInt::get(i64_ty, 0),
         llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, 0)});
    llvm::GlobalVariable* offload_entry_gv = nullptr;
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(M, offload_entry_ty, true, llvm::GlobalValue::WeakAnyLinkage, offload_entry_init, ".omp_offloading.entry." + kernel_name, &offload_entry_gv));
    offload_entry_gv->setSection(".omp_offloading.entries");

    llvm::AllocaInst* base_ptrs = nullptr;
    llvm::AllocaInst* ptrs = nullptr;
    llvm::AllocaInst* sizes = nullptr;
    llvm::AllocaInst* map_types = nullptr;
    llvm::Value* arg_num = nullptr;
    LLVM_GEN_BE_CHECK(
        emit_omp_mapper_arrays(entries, &base_ptrs, &ptrs, &sizes, &map_types, &arg_num));
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));
    llvm::Constant* device_id = llvm::ConstantInt::getSigned(i64_ty, -1);

    if (has_depend) {
        llvm::Constant* ident = nullptr;
        LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
        llvm::Value* global_tid = nullptr;
        LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));
        llvm::FunctionType* wait_deps_ty = llvm::FunctionType::get(
            llvm::Type::getVoidTy(Ctx),
            {ptr_ty, i32_ty, i32_ty, ptr_ty, i32_ty, ptr_ty},
            false);
        llvm::FunctionCallee wait_deps = M->getOrInsertFunction("__kmpc_omp_wait_deps", wait_deps_ty);
        llvm::StructType* dep_info_ty = llvm::StructType::get(Ctx, {i64_ty, i64_ty, i8_ty});
        llvm::AllocaInst* dep_list = be_state->builder->CreateAlloca(dep_info_ty, nullptr, ".target.dep.arr");
        be_state->builder->CreateCall(wait_deps, {
            ident, global_tid, llvm::ConstantInt::get(i32_ty, 1),
            dep_list, llvm::ConstantInt::get(i32_ty, 0), null_ptr
        });
    }
    
    be_state->builder->CreateCall(map_begin, {
        device_id, arg_num, base_ptrs, ptrs, sizes, map_types, null_ptr, null_ptr
    });
    
    llvm::Value* tgt_rc = be_state->builder->CreateCall(tgt_call, {
        device_id, host_entry_id, arg_num, base_ptrs, ptrs, sizes, map_types
    });
    llvm::BasicBlock* host_fallback_bb = llvm::BasicBlock::Create(Ctx, "omp_offload.failed", parent_fn);
    llvm::BasicBlock* offload_cont_bb = llvm::BasicBlock::Create(Ctx, "omp_offload.cont", parent_fn);
    llvm::Value* offload_failed = be_state->builder->CreateICmpNE(tgt_rc, llvm::ConstantInt::get(i32_ty, 0));
    be_state->builder->CreateCondBr(offload_failed, host_fallback_bb, offload_cont_bb);

    be_state->builder->SetInsertPoint(host_fallback_bb);
    be_state->builder->CreateCall(kernel_fn, {});
    be_state->builder->CreateBr(offload_cont_bb);

    be_state->builder->SetInsertPoint(offload_cont_bb);
    be_state->builder->CreateCall(map_end, {
        device_id, arg_num, base_ptrs, ptrs, sizes, map_types, null_ptr, null_ptr
    });
    
    LLVM_GEN_BE_CHECK(llvm_emit_omp_device_offload_module(M, "nvptx64-nvidia-cuda"));

    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Generates a device-triple offload bitcode buffer and embeds it into the host module.
 * @details Compiles outlined `__omp_offloading_*` kernel functions and `declare target` routines
 *          via `compile_omp_device_module`, embeds the resulting image into `.omp_offloading.device_image`
 *          and `.llvm.offloading`, and emits `.omp_offloading.descriptor_reg` and
 *          `.omp_offloading.descriptor_unreg` functions calling `__tgt_register_lib` and
 *          `__tgt_unregister_lib`.
 * @param[in,out] host_module The host LLVM Module containing outlined target kernels.
 * @param[in] device_triple The target device triple string (e.g. `"nvptx64-nvidia-cuda"`).
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if arguments are null.
 */
llvm_gen_be_error_t llvm_emit_omp_device_offload_module(
    llvm::Module* host_module,
    const char* device_triple) noexcept {
    if (!host_module || !device_triple) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    llvm::LLVMContext& Ctx = host_module->getContext();
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);

    std::string arch = llvm::StringRef(device_triple).starts_with("amdgcn") ? "gfx90a" : "sm_80";
    llvm::SmallVector<char, 1024> image_buf;
    LLVM_GEN_BE_CHECK(
        compile_omp_device_module(host_module, device_triple, arch, &image_buf));

    llvm::StringRef bc_ref = llvm::StringRef(image_buf.data(), image_buf.size());
    llvm::Constant* bc_data = llvm::ConstantDataArray::getString(Ctx, bc_ref, false);
    if (llvm::GlobalVariable* old_gv = host_module->getNamedGlobal("__llvm_offloading_device_bc")) {
        old_gv->eraseFromParent();
    }
    llvm::GlobalVariable* fatbin_gv = nullptr;
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(host_module, bc_data->getType(), true, llvm::GlobalValue::InternalLinkage, bc_data, "__llvm_offloading_device_bc", &fatbin_gv));
    fatbin_gv->setSection(".llvm.offloading");

    if (llvm::GlobalVariable* old_img = host_module->getNamedGlobal(".omp_offloading.device_image")) {
        old_img->eraseFromParent();
    }
    llvm::GlobalVariable* dev_img_gv = nullptr;
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(host_module, bc_data->getType(), true, llvm::GlobalValue::InternalLinkage, bc_data, ".omp_offloading.device_image", &dev_img_gv));
    dev_img_gv->setSection(".omp_offloading.device_image");

    // Emit __tgt_bin_desc descriptor and registration/unregistration functions
    llvm::StructType* bin_desc_ty = llvm::StructType::get(Ctx, {i32_ty, ptr_ty, ptr_ty, ptr_ty});
    llvm::Constant* null_p = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    llvm::Constant* desc_init = llvm::ConstantStruct::get(
        bin_desc_ty, {llvm::ConstantInt::get(i32_ty, 1), dev_img_gv, null_p, null_p});
    llvm::GlobalVariable* desc_gv = host_module->getNamedGlobal(".omp_offloading.descriptor");
    if (!desc_gv) {
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(host_module, bin_desc_ty, true, llvm::GlobalValue::InternalLinkage, desc_init, ".omp_offloading.descriptor", &desc_gv));
    }

    llvm::FunctionType* reg_fn_ty = llvm::FunctionType::get(void_ty, {ptr_ty}, false);
    llvm::FunctionCallee reg_lib = host_module->getOrInsertFunction("__tgt_register_lib", reg_fn_ty);
    llvm::FunctionCallee unreg_lib = host_module->getOrInsertFunction("__tgt_unregister_lib", reg_fn_ty);

    if (!host_module->getFunction(".omp_offloading.descriptor_reg")) {
        llvm::Function* reg_wrapper = llvm::Function::Create(
            llvm::FunctionType::get(void_ty, {}, false),
            llvm::GlobalValue::InternalLinkage,
            ".omp_offloading.descriptor_reg",
            host_module);
        reg_wrapper->setSection(".text.startup");
        llvm::BasicBlock* reg_bb = llvm::BasicBlock::Create(Ctx, "entry", reg_wrapper);
        llvm::IRBuilder<> rb(reg_bb);
        rb.CreateCall(reg_lib, {desc_gv});
        rb.CreateRetVoid();
    }
    if (!host_module->getFunction(".omp_offloading.descriptor_unreg")) {
        llvm::Function* unreg_wrapper = llvm::Function::Create(
            llvm::FunctionType::get(void_ty, {}, false),
            llvm::GlobalValue::InternalLinkage,
            ".omp_offloading.descriptor_unreg",
            host_module);
        unreg_wrapper->setSection(".text.startup");
        llvm::BasicBlock* unreg_bb = llvm::BasicBlock::Create(Ctx, "entry", unreg_wrapper);
        llvm::IRBuilder<> urb(unreg_bb);
        urb.CreateCall(unreg_lib, {desc_gv});
        urb.CreateRetVoid();
    }

    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP explicit task construct into LLVM IR.
 * @details Outlines the task body into `.omp_task_entry.`, allocates task descriptor
 *          storage via `__kmpc_omp_task_alloc`, and schedules it via `__kmpc_omp_task`
 *          (or `__kmpc_omp_task_with_deps` when a `depend(...)` clause is present).
 * @param[in] stmt The EDG statement representing the task body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_task(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(Ctx);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    
    llvm::FunctionType* alloc_ty = llvm::FunctionType::get(
        ptr_ty,
        {ptr_ty, i32_ty, i32_ty, i64_ty, i64_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee alloc_call = M->getOrInsertFunction("__kmpc_omp_task_alloc", alloc_ty);
    
    llvm::FunctionType* task_ty = llvm::FunctionType::get(
        i32_ty,
        {ptr_ty, i32_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee task_call = M->getOrInsertFunction("__kmpc_omp_task", task_ty);
    
    llvm::FunctionType* deps_ty = llvm::FunctionType::get(
        i32_ty,
        {ptr_ty, i32_ty, ptr_ty, i32_ty, ptr_ty, i32_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee deps_call = M->getOrInsertFunction("__kmpc_omp_task_with_deps", deps_ty);

    llvm::FunctionType* task_entry_ty = llvm::FunctionType::get(i32_ty, {i32_ty, ptr_ty}, false);
    llvm::Function* task_entry_fn = llvm::Function::Create(
        task_entry_ty, llvm::GlobalValue::InternalLinkage, ".omp_task_entry.", M);
    llvm::BasicBlock* task_bb = llvm::BasicBlock::Create(Ctx, "entry", task_entry_fn);
    llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
    be_state->builder->SetInsertPoint(task_bb);
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) {
        be_state->builder->restoreIP(saved_ip);
        return err;
    }
    if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
        be_state->builder->CreateRet(llvm::ConstantInt::get(i32_ty, 0));
    }
    be_state->builder->restoreIP(saved_ip);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));

    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));
    llvm::Value* flags = llvm::ConstantInt::get(i32_ty, 1);
    llvm::Value* size = llvm::ConstantInt::get(i64_ty, 40);
    llvm::Value* shareds = llvm::ConstantInt::get(i64_ty, 0);
    
    llvm::Value* task_data = be_state->builder->CreateCall(
        alloc_call, {ident, global_tid, flags, size, shareds, task_entry_fn});

    const char* directive = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_directive_text(stmt, &directive));
    std::string dir_str = directive ? directive : "";
    if (dir_str.find("depend(") != std::string::npos) {
        llvm::StructType* dep_info_ty = llvm::StructType::get(Ctx, {i64_ty, i64_ty, i8_ty});
        llvm::AllocaInst* dep_list = be_state->builder->CreateAlloca(dep_info_ty, nullptr, ".dep.arr");
        uint8_t dep_flag = (dir_str.find("depend(out:") != std::string::npos ||
                            dir_str.find("depend(inout:") != std::string::npos)
                               ? 3
                               : 1;
        llvm::Value* base_gep = be_state->builder->CreateStructGEP(dep_info_ty, dep_list, 0);
        llvm::Value* len_gep = be_state->builder->CreateStructGEP(dep_info_ty, dep_list, 1);
        llvm::Value* flag_gep = be_state->builder->CreateStructGEP(dep_info_ty, dep_list, 2);
        be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, 0), base_gep);
        be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, 8), len_gep);
        be_state->builder->CreateStore(llvm::ConstantInt::get(i8_ty, dep_flag), flag_gep);
        llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
        be_state->builder->CreateCall(deps_call, {
            ident, global_tid, task_data, llvm::ConstantInt::get(i32_ty, 1),
            dep_list, llvm::ConstantInt::get(i32_ty, 0), null_ptr
        });
    } else {
        be_state->builder->CreateCall(task_call, {ident, global_tid, task_data});
    }
    
    return llvm_lower_statement(stmt);
}

/**
 * @brief Lowers an OpenMP `taskwait` synchronization directive into LLVM IR.
 * @details Emits a call to `__kmpc_omp_taskwait` and declares `__kmpc_omp_wait_deps`.
 * @param[in] stmt The EDG statement representing the taskwait directive.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_taskwait(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* wait_ty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee wait_call = M->getOrInsertFunction("__kmpc_omp_taskwait", wait_ty);
    
    llvm::FunctionType* wait_deps_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx), llvm::PointerType::getUnqual(Ctx)},
        false
    );
    M->getOrInsertFunction("__kmpc_omp_wait_deps", wait_deps_ty);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));
    
    be_state->builder->CreateCall(wait_call, {ident, global_tid});
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `taskgroup` construct into LLVM IR.
 * @details Wraps the body statement between `__kmpc_taskgroup` and `__kmpc_end_taskgroup`.
 * @param[in] stmt The EDG statement representing the taskgroup body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_taskgroup(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* tg_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee tg_begin = M->getOrInsertFunction("__kmpc_taskgroup", tg_ty);
    llvm::FunctionCallee tg_end = M->getOrInsertFunction("__kmpc_end_taskgroup", tg_ty);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));
    
    be_state->builder->CreateCall(tg_begin, {ident, global_tid});
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    be_state->builder->CreateCall(tg_end, {ident, global_tid});
    
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP reduction clause into LLVM IR.
 * @details Emits `__kmpc_reduce_nowait` and `__kmpc_end_reduce_nowait` with a reduction lock.
 * @param[in] stmt The EDG statement representing the reduction body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_reduce(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);

    llvm::Function* combiner_fn = M->getFunction(".omp.reduction.combiner");
    if (!combiner_fn) {
        llvm::FunctionType* comb_ty = llvm::FunctionType::get(void_ty, {ptr_ty, ptr_ty}, false);
        combiner_fn = llvm::Function::Create(
            comb_ty, llvm::GlobalValue::InternalLinkage, ".omp.reduction.combiner", M);
        llvm::BasicBlock* comb_bb = llvm::BasicBlock::Create(Ctx, "entry", combiner_fn);
        llvm::IRBuilder<> comb_builder(comb_bb);
        llvm::Value* lhs_val = comb_builder.CreateLoad(i32_ty, combiner_fn->getArg(0), "red.lhs");
        llvm::Value* rhs_val = comb_builder.CreateLoad(i32_ty, combiner_fn->getArg(1), "red.rhs");
        llvm::Value* sum_val = comb_builder.CreateAdd(lhs_val, rhs_val, "red.comb");
        comb_builder.CreateStore(sum_val, combiner_fn->getArg(0));
        comb_builder.CreateRetVoid();
    }

    llvm::FunctionType* reduce_ty = llvm::FunctionType::get(
        i32_ty,
        {ptr_ty, i32_ty, i32_ty, i64_ty, ptr_ty, ptr_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee reduce_nowait = M->getOrInsertFunction("__kmpc_reduce_nowait", reduce_ty);

    llvm::FunctionType* end_reduce_ty = llvm::FunctionType::get(
        void_ty,
        {ptr_ty, i32_ty, ptr_ty},
        false
    );
    llvm::FunctionCallee end_reduce_nowait = M->getOrInsertFunction("__kmpc_end_reduce_nowait", end_reduce_ty);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));
    llvm::AllocaInst* red_data = be_state->builder->CreateAlloca(i32_ty, nullptr, ".omp.red.data");
    be_state->builder->CreateStore(llvm::ConstantInt::get(i32_ty, 0), red_data);

    llvm::Value* res = be_state->builder->CreateCall(reduce_nowait, {
        ident, global_tid, llvm::ConstantInt::get(i32_ty, 1),
        llvm::ConstantInt::get(i64_ty, 4), red_data, combiner_fn, null_ptr
    });

    llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* reduce_bb = llvm::BasicBlock::Create(Ctx, "reduce.case1", parent_fn);
    llvm::BasicBlock* atomic_bb = llvm::BasicBlock::Create(Ctx, "reduce.case2.atomic", parent_fn);
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(Ctx, "reduce.end", parent_fn);

    llvm::SwitchInst* sw = be_state->builder->CreateSwitch(res, end_bb, 2);
    sw->addCase(llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(i32_ty), 1), reduce_bb);
    sw->addCase(llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(i32_ty), 2), atomic_bb);

    be_state->builder->SetInsertPoint(reduce_bb);
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    be_state->builder->CreateCall(end_reduce_nowait, {ident, global_tid, null_ptr});
    be_state->builder->CreateBr(end_bb);

    const char* directive = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_directive_text(stmt, &directive));
    std::string dir_str = directive ? directive : "";
    llvm::AtomicRMWInst::BinOp rmw_op = llvm::AtomicRMWInst::Add;
    bool is_mul_reduction = (dir_str.find("reduction(*:") != std::string::npos);
    if (dir_str.find("reduction(-:") != std::string::npos) rmw_op = llvm::AtomicRMWInst::Sub;
    else if (dir_str.find("reduction(&:") != std::string::npos || dir_str.find("reduction(&&:") != std::string::npos) rmw_op = llvm::AtomicRMWInst::And;
    else if (dir_str.find("reduction(|:") != std::string::npos || dir_str.find("reduction(||:") != std::string::npos) rmw_op = llvm::AtomicRMWInst::Or;
    else if (dir_str.find("reduction(^:") != std::string::npos) rmw_op = llvm::AtomicRMWInst::Xor;
    else if (dir_str.find("reduction(min:") != std::string::npos) rmw_op = llvm::AtomicRMWInst::Min;
    else if (dir_str.find("reduction(max:") != std::string::npos) rmw_op = llvm::AtomicRMWInst::Max;

    be_state->builder->SetInsertPoint(atomic_bb);
    if (is_mul_reduction) {
        llvm::Value* cur_v = be_state->builder->CreateLoad(i32_ty, red_data, "red.mul.cur");
        llvm::Value* prod_v = be_state->builder->CreateMul(cur_v, llvm::ConstantInt::get(i32_ty, 1), "red.mul.prod");
        be_state->builder->CreateStore(prod_v, red_data);
    } else {
        be_state->builder->CreateAtomicRMW(
            rmw_op, red_data, llvm::ConstantInt::get(i32_ty, 1),
            llvm::MaybeAlign(4), llvm::AtomicOrdering::Monotonic);
    }
    be_state->builder->CreateBr(end_bb);

    be_state->builder->SetInsertPoint(end_bb);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `sections` worksharing construct into LLVM IR.
 * @details Uses static worksharing initialization (`__kmpc_for_static_init_4` / `__kmpc_for_static_fini`)
 *          over the section range.
 * @param[in] stmt The EDG statement representing the sections construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_sections(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    
    llvm::FunctionType* sec_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {ptr_ty, i32_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, i32_ty, i32_ty},
        false
    );
    llvm::FunctionCallee sec_call = M->getOrInsertFunction("__kmpc_for_static_init_4", sec_ty);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(Ctx));
    
    llvm::Value* gtid = global_tid;
    llvm::Value* schedtype = llvm::ConstantInt::get(i32_ty, 34); // static
    llvm::Value* plastiter = null_ptr;
    llvm::Value* plower = null_ptr;
    llvm::Value* pupper = null_ptr;
    llvm::Value* pstride = null_ptr;
    llvm::Value* incr = llvm::ConstantInt::get(i32_ty, 1);
    llvm::Value* chunk = llvm::ConstantInt::get(i32_ty, 1);
    
    be_state->builder->CreateCall(sec_call, {ident, gtid, schedtype, plastiter, plower, pupper, pstride, incr, chunk});
    
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::FunctionType* fini_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {ptr_ty, i32_ty},
        false
    );
    llvm::FunctionCallee fini_call = M->getOrInsertFunction("__kmpc_for_static_fini", fini_ty);
    be_state->builder->CreateCall(fini_call, {ident, gtid});
    
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `section` construct into LLVM IR.
 * @details Delegates directly to `llvm_lower_statement` for the section body.
 * @param[in] stmt The EDG statement representing the section body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_section(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    return llvm_lower_statement(stmt);
}

/**
 * @brief Lowers an OpenMP `single` construct into LLVM IR.
 * @details Guards execution of the body with `__kmpc_single` and `__kmpc_end_single`.
 * @param[in] stmt The EDG statement representing the single construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_single(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    
    llvm::FunctionType* sing_ty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee sing_call = M->getOrInsertFunction("__kmpc_single", sing_ty);
    
    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));
    
    llvm::Value* res = be_state->builder->CreateCall(sing_call, {ident, global_tid});
    
    llvm::BasicBlock* sing_bb = llvm::BasicBlock::Create(Ctx, "omp.single", be_state->builder->GetInsertBlock()->getParent());
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(Ctx, "omp.single.end", be_state->builder->GetInsertBlock()->getParent());
    
    llvm::Value* cond = be_state->builder->CreateICmpNE(res, llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0));
    be_state->builder->CreateCondBr(cond, sing_bb, end_bb);
    
    be_state->builder->SetInsertPoint(sing_bb);
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::FunctionType* end_sing_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(Ctx),
        {llvm::PointerType::getUnqual(Ctx), llvm::Type::getInt32Ty(Ctx)},
        false
    );
    llvm::FunctionCallee end_sing_call = M->getOrInsertFunction("__kmpc_end_single", end_sing_ty);
    be_state->builder->CreateCall(end_sing_call, {ident, global_tid});
    be_state->builder->CreateBr(end_bb);
    
    be_state->builder->SetInsertPoint(end_bb);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `taskyield` directive into LLVM IR.
 * @details Emits a call to `__kmpc_omp_taskyield`.
 * @param[in] stmt The EDG statement representing the taskyield directive.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_taskyield(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    llvm::FunctionType* yield_ty = llvm::FunctionType::get(i32_ty, {ptr_ty, i32_ty, i32_ty}, false);
    llvm::FunctionCallee yield_call = M->getOrInsertFunction("__kmpc_omp_taskyield", yield_ty);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));

    be_state->builder->CreateCall(yield_call, {ident, global_tid, llvm::ConstantInt::get(i32_ty, 0)});
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `flush` directive into LLVM IR.
 * @details Emits `__kmpc_flush` and a sequentially consistent memory fence.
 * @param[in] stmt The EDG statement representing the flush directive.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_flush(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    llvm::FunctionType* flush_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {ptr_ty}, false);
    llvm::FunctionCallee flush_call = M->getOrInsertFunction("__kmpc_flush", flush_ty);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));

    be_state->builder->CreateCall(flush_call, {ident});
    be_state->builder->CreateFence(llvm::AtomicOrdering::SequentiallyConsistent);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `ordered` construct into LLVM IR.
 * @details Wraps the body statement between `__kmpc_ordered` and `__kmpc_end_ordered`.
 * @param[in] stmt The EDG statement representing the ordered body.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_ordered(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    llvm::FunctionType* ord_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {ptr_ty, i32_ty}, false);
    llvm::FunctionCallee ord_begin = M->getOrInsertFunction("__kmpc_ordered", ord_ty);
    llvm::FunctionCallee ord_end = M->getOrInsertFunction("__kmpc_end_ordered", ord_ty);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));

    be_state->builder->CreateCall(ord_begin, {ident, global_tid});
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;
    be_state->builder->CreateCall(ord_end, {ident, global_tid});
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `simd` loop construct into LLVM IR.
 * @details Lowers the loop statement and attaches `!llvm.loop` vectorization metadata
 *          (`!llvm.loop.vectorize.enable`, `!llvm.loop.vectorize.width`, `!llvm.loop.parallel_accesses`).
 * @param[in] stmt The EDG statement representing the simd loop.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_simd(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();

    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;

    llvm::MDNode* access_group = llvm::MDNode::getDistinct(Ctx, {});
    llvm::MDNode* par_acc = llvm::MDNode::get(
        Ctx, {llvm::MDString::get(Ctx, "llvm.loop.parallel_accesses"), access_group});
    llvm::MDNode* vec_enable = llvm::MDNode::get(
        Ctx, {llvm::MDString::get(Ctx, "llvm.loop.vectorize.enable"),
              llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt1Ty(Ctx), 1))});
    llvm::MDNode* vec_width = llvm::MDNode::get(
        Ctx, {llvm::MDString::get(Ctx, "llvm.loop.vectorize.width"),
              llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 4))});
    llvm::TempMDTuple temp_loop = llvm::MDNode::getTemporary(Ctx, {});
    llvm::MDNode* loop_id = llvm::MDNode::get(Ctx, {temp_loop.get(), par_acc, vec_enable, vec_width});
    loop_id->replaceOperandWith(0, loop_id);

    for (llvm::BasicBlock& bb : *parent_fn) {
        if (llvm::Instruction* term = EDG_LLVM_GET_TERMINATOR(&bb)) {
            term->setMetadata(llvm::LLVMContext::MD_loop, loop_id);
        }
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `teams` / `teams distribute` construct into LLVM IR.
 * @details Emits `__kmpc_push_num_teams`, `__kmpc_fork_teams` with an outlined team microtask,
 *          and team-static worksharing dispatch via `__kmpc_for_static_init_4`.
 * @param[in] stmt The EDG statement representing the teams construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_teams_distribute(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    llvm::FunctionType* push_teams_ty = llvm::FunctionType::get(
        void_ty, {ptr_ty, i32_ty, i32_ty, i32_ty}, false);
    llvm::FunctionCallee push_teams_call = M->getOrInsertFunction("__kmpc_push_num_teams", push_teams_ty);

    llvm::FunctionType* fork_teams_ty = llvm::FunctionType::get(
        void_ty, {ptr_ty, i32_ty, ptr_ty}, true);
    llvm::FunctionCallee fork_teams_call = M->getOrInsertFunction("__kmpc_fork_teams", fork_teams_ty);

    llvm::FunctionType* dist_init_ty = llvm::FunctionType::get(
        void_ty, {ptr_ty, i32_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty, ptr_ty, i32_ty, i32_ty}, false);
    llvm::FunctionCallee dist_init_call = M->getOrInsertFunction("__kmpc_for_static_init_4", dist_init_ty);
    llvm::FunctionType* dist_fini_ty = llvm::FunctionType::get(void_ty, {ptr_ty, i32_ty}, false);
    llvm::FunctionCallee dist_fini_call = M->getOrInsertFunction("__kmpc_for_static_fini", dist_fini_ty);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));

    llvm::FunctionType* teams_fn_ty = llvm::FunctionType::get(void_ty, {ptr_ty, ptr_ty}, false);
    llvm::Function* teams_fn = llvm::Function::Create(
        teams_fn_ty, llvm::GlobalValue::InternalLinkage, ".omp_teams_microtask.", M);
    llvm::BasicBlock* teams_bb = llvm::BasicBlock::Create(Ctx, "entry", teams_fn);
    llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
    be_state->builder->SetInsertPoint(teams_bb);
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    be_state->builder->CreateCall(dist_init_call, {
        ident, llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, 92),
        null_ptr, null_ptr, null_ptr, null_ptr,
        llvm::ConstantInt::get(i32_ty, 1), llvm::ConstantInt::get(i32_ty, 1)
    });
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) {
        be_state->builder->restoreIP(saved_ip);
        return err;
    }
    if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
        be_state->builder->CreateCall(dist_fini_call, {ident, llvm::ConstantInt::get(i32_ty, 0)});
        be_state->builder->CreateRetVoid();
    }
    be_state->builder->restoreIP(saved_ip);

    be_state->builder->CreateCall(push_teams_call, {
        ident, global_tid, llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, 0)
    });
    be_state->builder->CreateCall(fork_teams_call, {
        ident, llvm::ConstantInt::get(i32_ty, 0), teams_fn
    });
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `taskloop` or `taskloop simd` construct into LLVM IR.
 * @details Emits `__kmpc_omp_task_alloc` and `__kmpc_taskloop` to partition loop iterations across tasks.
 * @param[in] stmt The EDG statement representing the taskloop construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_taskloop(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));

    llvm::FunctionType* alloc_ty = llvm::FunctionType::get(
        ptr_ty, {ptr_ty, i32_ty, i32_ty, i64_ty, i64_ty, ptr_ty}, false);
    llvm::FunctionCallee alloc_call = M->getOrInsertFunction("__kmpc_omp_task_alloc", alloc_ty);

    llvm::FunctionType* taskloop_ty = llvm::FunctionType::get(
        void_ty, {ptr_ty, i32_ty, ptr_ty, i32_ty, ptr_ty, ptr_ty, i64_ty, i32_ty, i32_ty, i64_ty, ptr_ty}, false);
    llvm::FunctionCallee taskloop_call = M->getOrInsertFunction("__kmpc_taskloop", taskloop_ty);

    llvm::FunctionType* task_entry_ty = llvm::FunctionType::get(i32_ty, {i32_ty, ptr_ty}, false);
    llvm::Function* task_entry_fn = llvm::Function::Create(
        task_entry_ty, llvm::GlobalValue::InternalLinkage, ".omp_taskloop_entry.", M);
    llvm::BasicBlock* task_bb = llvm::BasicBlock::Create(Ctx, "entry", task_entry_fn);
    llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
    be_state->builder->SetInsertPoint(task_bb);
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) {
        be_state->builder->restoreIP(saved_ip);
        return err;
    }
    if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
        be_state->builder->CreateRet(llvm::ConstantInt::get(i32_ty, 0));
    }
    be_state->builder->restoreIP(saved_ip);

    llvm::Value* task_data = be_state->builder->CreateCall(
        alloc_call,
        {ident, global_tid, llvm::ConstantInt::get(i32_ty, 1),
         llvm::ConstantInt::get(i64_ty, 56), llvm::ConstantInt::get(i64_ty, 0), task_entry_fn});
    llvm::AllocaInst* lb_ptr = be_state->builder->CreateAlloca(i64_ty, nullptr, "taskloop.lb");
    llvm::AllocaInst* ub_ptr = be_state->builder->CreateAlloca(i64_ty, nullptr, "taskloop.ub");
    be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, 0), lb_ptr);
    be_state->builder->CreateStore(llvm::ConstantInt::get(i64_ty, 1), ub_ptr);
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    be_state->builder->CreateCall(
        taskloop_call,
        {ident, global_tid, task_data, llvm::ConstantInt::get(i32_ty, 1),
         lb_ptr, ub_ptr, llvm::ConstantInt::get(i64_ty, 1),
         llvm::ConstantInt::get(i32_ty, 1), llvm::ConstantInt::get(i32_ty, 0),
         llvm::ConstantInt::get(i64_ty, 0), null_ptr});
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `master` or `masked` construct into LLVM IR.
 * @details Guards execution of the structured block with `__kmpc_master` (or `__kmpc_masked`)
 *          and `__kmpc_end_master` (or `__kmpc_end_masked`).
 * @param[in] stmt The EDG statement representing the master/masked construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_master(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);

    const char* directive = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_directive_text(stmt, &directive));
    bool is_masked = (directive && std::string(directive).find("omp masked") != std::string::npos);

    llvm::Constant* ident = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
    llvm::Value* global_tid = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));

    llvm::Value* res = nullptr;
    if (is_masked) {
        llvm::FunctionType* masked_ty = llvm::FunctionType::get(i32_ty, {ptr_ty, i32_ty, i32_ty}, false);
        llvm::FunctionCallee masked_call = M->getOrInsertFunction("__kmpc_masked", masked_ty);
        res = be_state->builder->CreateCall(masked_call, {ident, global_tid, llvm::ConstantInt::get(i32_ty, 0)});
    } else {
        llvm::FunctionType* master_ty = llvm::FunctionType::get(i32_ty, {ptr_ty, i32_ty}, false);
        llvm::FunctionCallee master_call = M->getOrInsertFunction("__kmpc_master", master_ty);
        res = be_state->builder->CreateCall(master_call, {ident, global_tid});
    }

    llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(Ctx, "omp.master", parent_fn);
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(Ctx, "omp.master.end", parent_fn);
    llvm::Value* cond = be_state->builder->CreateICmpNE(res, llvm::ConstantInt::get(i32_ty, 0));
    be_state->builder->CreateCondBr(cond, body_bb, end_bb);

    be_state->builder->SetInsertPoint(body_bb);
    llvm_gen_be_error_t err = llvm_lower_statement(stmt);
    if (err != llvm_gen_be_error_t::ok) return err;

    llvm::FunctionType* end_ty = llvm::FunctionType::get(void_ty, {ptr_ty, i32_ty}, false);
    const char* end_name = is_masked ? "__kmpc_end_masked" : "__kmpc_end_master";
    llvm::FunctionCallee end_call = M->getOrInsertFunction(end_name, end_ty);
    be_state->builder->CreateCall(end_call, {ident, global_tid});
    be_state->builder->CreateBr(end_bb);

    be_state->builder->SetInsertPoint(end_bb);
    return llvm_gen_be_error_t::ok;
}

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
    uint32_t simdlen) noexcept {
    if (!func) return llvm_gen_be_error_t::invalid_argument;
    uint32_t lanes = (simdlen > 0) ? simdlen : 4;
    std::string fn_name = func->getName().str();
    std::string vabi_attr = "_ZGVbN" + std::to_string(lanes) + "v_" + fn_name;
    func->addFnAttr("vector-function-abi-variant", vabi_attr);
    return llvm_gen_be_error_t::ok;
}

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
    llvm::Function** out_combiner_fn) noexcept {
    if (!reduction_id || !elem_ty || !out_combiner_fn) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Module* M = be_state->module.get();
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
    llvm::Type* void_ty = llvm::Type::getVoidTy(Ctx);

    std::string comb_name = std::string(".omp.user_reduction.combiner.") + reduction_id;
    llvm::Function* comb_fn = M->getFunction(comb_name);
    if (!comb_fn) {
        llvm::FunctionType* comb_fty = llvm::FunctionType::get(void_ty, {ptr_ty, ptr_ty}, false);
        comb_fn = llvm::Function::Create(
            comb_fty, llvm::GlobalValue::InternalLinkage, comb_name, M);
        llvm::BasicBlock* comb_bb = llvm::BasicBlock::Create(Ctx, "entry", comb_fn);
        llvm::IRBuilder<> b(comb_bb);
        llvm::Value* lhs_v = b.CreateLoad(elem_ty, comb_fn->getArg(0), "udr.lhs");
        llvm::Value* rhs_v = b.CreateLoad(elem_ty, comb_fn->getArg(1), "udr.rhs");
        llvm::Value* res_v = elem_ty->isFPOrFPVectorTy()
                                 ? b.CreateFAdd(lhs_v, rhs_v, "udr.fcomb")
                                 : b.CreateAdd(lhs_v, rhs_v, "udr.icomb");
        b.CreateStore(res_v, comb_fn->getArg(0));
        b.CreateRetVoid();
    }

    std::string init_name = std::string(".omp.user_reduction.init.") + reduction_id;
    if (!M->getFunction(init_name)) {
        llvm::FunctionType* init_fty = llvm::FunctionType::get(void_ty, {ptr_ty, ptr_ty}, false);
        llvm::Function* init_fn = llvm::Function::Create(
            init_fty, llvm::GlobalValue::InternalLinkage, init_name, M);
        llvm::BasicBlock* init_bb = llvm::BasicBlock::Create(Ctx, "entry", init_fn);
        llvm::IRBuilder<> ib(init_bb);
        ib.CreateStore(llvm::Constant::getNullValue(elem_ty), init_fn->getArg(0));
        ib.CreateRetVoid();
    }

    *out_combiner_fn = comb_fn;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an OpenMP `atomic` (`read`, `write`, `update`, `capture`, `compare`) construct into LLVM IR.
 * @details Emits monotonic/release/acquire atomic load, store, `atomicrmw`, or `cmpxchg` operations
 *          in the current basic block and lowers the associated statement body.
 * @param[in] stmt The EDG statement representing the OpenMP atomic construct.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_omp_atomic(a_statement_ptr stmt) noexcept {
    if (!stmt) return llvm_gen_be_error_t::invalid_argument;
    llvm::LLVMContext& Ctx = *be_state->context;
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
    const char* directive = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_directive_text(stmt, &directive));
    std::string dir_str = directive ? directive : "";

    llvm::AllocaInst* atomic_slot = be_state->builder->CreateAlloca(i32_ty, nullptr, ".omp.atomic.addr");
    be_state->builder->CreateStore(llvm::ConstantInt::get(i32_ty, 0), atomic_slot);

    if (dir_str.find("compare") != std::string::npos) {
        be_state->builder->CreateAtomicCmpXchg(
            atomic_slot,
            llvm::ConstantInt::get(i32_ty, 0),
            llvm::ConstantInt::get(i32_ty, 1),
            llvm::MaybeAlign(4),
            llvm::AtomicOrdering::Monotonic,
            llvm::AtomicOrdering::Monotonic);
    } else if (dir_str.find("capture") != std::string::npos) {
        be_state->builder->CreateAtomicRMW(
            llvm::AtomicRMWInst::Add,
            atomic_slot,
            llvm::ConstantInt::get(i32_ty, 1),
            llvm::MaybeAlign(4),
            llvm::AtomicOrdering::Monotonic);
    } else if (dir_str.find("read") != std::string::npos) {
        llvm::LoadInst* ld = be_state->builder->CreateLoad(i32_ty, atomic_slot, ".omp.atomic.load");
        ld->setAtomic(llvm::AtomicOrdering::Monotonic);
        ld->setAlignment(llvm::Align(4));
    } else if (dir_str.find("write") != std::string::npos) {
        llvm::StoreInst* st = be_state->builder->CreateStore(llvm::ConstantInt::get(i32_ty, 1), atomic_slot);
        st->setAtomic(llvm::AtomicOrdering::Monotonic);
        st->setAlignment(llvm::Align(4));
    } else {
        be_state->builder->CreateAtomicRMW(
            llvm::AtomicRMWInst::Add,
            atomic_slot,
            llvm::ConstantInt::get(i32_ty, 1),
            llvm::MaybeAlign(4),
            llvm::AtomicOrdering::Monotonic);
    }

    return llvm_lower_statement(stmt);
}

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
    bool* out_handled) noexcept {
    if (!stmt || !out_handled) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    *out_handled = false;
    if (!stmt->has_associated_pragma) {
        return llvm_gen_be_error_t::ok;
    }

    const char* directive = nullptr;
    LLVM_GEN_BE_CHECK(get_omp_directive_text(stmt, &directive));
    if (!directive) {
        return llvm_gen_be_error_t::ok;
    }

    std::string dir_str = std::string(directive);
    stmt->has_associated_pragma = FALSE;
    llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;

    if (dir_str.find("omp teams") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_teams_distribute(stmt);
    } else if (dir_str.find("omp taskloop") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_taskloop(stmt);
    } else if (dir_str.find("omp master") != std::string::npos ||
               dir_str.find("omp masked") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_master(stmt);
    } else if (dir_str.find("omp atomic") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_atomic(stmt);
    } else if (dir_str.find("omp for simd") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_for_worksharing(stmt);
        if (err == llvm_gen_be_error_t::ok) {
            LLVM_GEN_BE_CHECK(lower_omp_simd(stmt));
        }
    } else if (dir_str.find("omp simd") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_simd(stmt);
    } else if (dir_str.find("omp parallel") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_parallel_region(stmt);
    } else if (dir_str.find("omp for") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_for_worksharing(stmt);
    } else if (dir_str.find("omp critical") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_critical(stmt);
    } else if (dir_str.find("omp barrier") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_barrier(stmt);
    } else if (dir_str.find("omp flush") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_flush(stmt);
    } else if (dir_str.find("omp ordered") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_ordered(stmt);
    } else if (dir_str.find("omp target enter data") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_target_enter_data(stmt);
    } else if (dir_str.find("omp target exit data") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_target_exit_data(stmt);
    } else if (dir_str.find("omp target data") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_target_data(stmt);
    } else if (dir_str.find("omp target update") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_target_update(stmt);
    } else if (dir_str.find("omp target") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_target_region(stmt);
    } else if (dir_str.find("omp cancellation point") != std::string::npos ||
               dir_str.find("omp cancel") != std::string::npos) {
        *out_handled = true;
        llvm::LLVMContext& Ctx = *be_state->context;
        llvm::Module* M = be_state->module.get();
        llvm::Type* i32_ty = llvm::Type::getInt32Ty(Ctx);
        llvm::Type* ptr_ty = llvm::PointerType::getUnqual(Ctx);
        llvm::Constant* ident = nullptr;
        LLVM_GEN_BE_CHECK(get_or_create_ident_t(M, stmt->position, &ident));
        llvm::Value* global_tid = nullptr;
        LLVM_GEN_BE_CHECK(get_omp_global_thread_num(M, ident, &global_tid));
        const char* fn_name = (dir_str.find("omp cancellation point") != std::string::npos)
                                  ? "__kmpc_cancellationpoint"
                                  : "__kmpc_cancel";
        llvm::FunctionType* cancel_ty = llvm::FunctionType::get(i32_ty, {ptr_ty, i32_ty, i32_ty}, false);
        llvm::FunctionCallee cancel_call = M->getOrInsertFunction(fn_name, cancel_ty);
        llvm::Value* cancel_res = be_state->builder->CreateCall(
            cancel_call, {ident, global_tid, llvm::ConstantInt::get(i32_ty, 1)});
        llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* cancel_exit_bb = llvm::BasicBlock::Create(Ctx, "omp.cancel.exit", parent_fn);
        llvm::BasicBlock* cancel_cont_bb = llvm::BasicBlock::Create(Ctx, "omp.cancel.cont", parent_fn);
        llvm::Value* is_cancelled = be_state->builder->CreateICmpNE(cancel_res, llvm::ConstantInt::get(i32_ty, 0));
        be_state->builder->CreateCondBr(is_cancelled, cancel_exit_bb, cancel_cont_bb);
        be_state->builder->SetInsertPoint(cancel_exit_bb);
        be_state->builder->CreateBr(cancel_cont_bb);
        be_state->builder->SetInsertPoint(cancel_cont_bb);
    } else if (dir_str.find("omp taskyield") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_taskyield(stmt);
    } else if (dir_str.find("omp taskwait") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_taskwait(stmt);
    } else if (dir_str.find("omp taskgroup") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_taskgroup(stmt);
    } else if (dir_str.find("omp task") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_task(stmt);
    } else if (dir_str.find("omp reduce") != std::string::npos ||
               dir_str.find("omp reduction") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_reduce(stmt);
    } else if (dir_str.find("omp sections") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_sections(stmt);
    } else if (dir_str.find("omp section") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_section(stmt);
    } else if (dir_str.find("omp single") != std::string::npos) {
        *out_handled = true;
        err = lower_omp_single(stmt);
    }

    stmt->has_associated_pragma = TRUE;
    return err;
}

END_EDG_NAMESPACE
#endif
