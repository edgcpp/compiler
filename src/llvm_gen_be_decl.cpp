
/**
 * @file llvm_gen_be_decl.cpp
 * @ingroup llvm_gen_be_decl
 * @brief Declaration lowering subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end global variables, function prototypes, and
 * function bodies into LLVM IR globals and functions.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include <type_traits>
#include <vector>

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_abi_sysv_x86_64.h"
#include "llvm_gen_be_abi_aapcs64.h"
#include "llvm_gen_be_abi_win64.h"
#include "llvm_gen_be_abi_x86_32.h"
#include "llvm_gen_be_abi_arm32.h"
#include "llvm_gen_be_abi_wasm.h"
#include "llvm_gen_be_abi_riscv64.h"
#include "llvm_gen_be_abi_loongarch64.h"
#include "llvm_gen_be_abi_mips.h"
#include "llvm_gen_be_abi_ppc64.h"
#include "llvm_gen_be_abi_s390x.h"
#include "llvm_gen_be_abi_sparc.h"
#include "llvm_gen_be_eh.h"
#include "llvm_gen_be_vtable.h"
#include "il_read.h"
#include "llvm_gen_be_debug.h"
#if DO_IL_LOWERING
#include "lower_il.h"
#endif

#include <llvm/IR/Verifier.h>
#include <llvm/Support/MemoryBuffer.h>
#include "target.h"
#include "trans_unit.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Scans a statement subtree to attach `#pragma omp ...` source lines when `-fopenmp` is active.
 * @details Resolves each statement's source file and 1-based physical line via `conv_seq_to_file_and_line`,
 *          caches the source file's lines in `be_state->omp_source_file_cache`, and attaches any
 *          immediately preceding `#pragma omp` directive to the outermost statement on that line.
 * @param[in] stmt Root statement of the subtree to scan.
 * @param[in,out] claimed_lines Tracks `(file:line)` keys already claimed by an outer statement.
 * @return llvm_gen_be_error_t::ok on success, or `out_of_memory` on allocation failure.
 */
static llvm_gen_be_error_t scan_omp_pragmas_in_stmt_tree(
    a_statement_ptr stmt,
    std::unordered_set<std::string>* claimed_lines) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  for (a_statement_ptr cur = stmt; cur != nullptr; cur = cur->next) {
    if (cur->position.seq != 0 && be_state->omp_pragmas.find(cur) == be_state->omp_pragmas.end()) {
      a_const_char* short_fn = nullptr;
      a_const_char* full_fn = nullptr;
      a_line_number phys_line = 0;
      a_boolean at_eos = FALSE;
      conv_seq_to_file_and_line(cur->position.seq, &short_fn, &full_fn, &phys_line, &at_eos);
      const char* path_cstr = (full_fn && full_fn[0] != '\0') ? full_fn : short_fn;
      if (path_cstr && path_cstr[0] != '\0' && phys_line >= 2) {
        std::string claim_key = std::string(path_cstr) + ":" + std::to_string(phys_line);
        if (claimed_lines->insert(claim_key).second) {
          auto fit = be_state->omp_source_file_cache.find(path_cstr);
          if (fit == be_state->omp_source_file_cache.end()) {
            std::vector<std::string> lines;
            auto buf_or_err = llvm::MemoryBuffer::getFile(path_cstr);
            if (buf_or_err) {
              llvm::StringRef content = (*buf_or_err)->getBuffer();
              while (!content.empty()) {
                auto split_p = content.split('\n');
                llvm::StringRef ln = split_p.first.rtrim('\r');
                lines.push_back(ln.str());
                content = split_p.second;
              }
            }
            fit = be_state->omp_source_file_cache.emplace(path_cstr, std::move(lines)).first;
          }
          const std::vector<std::string>& src_lines = fit->second;
          size_t check_idx = static_cast<size_t>(phys_line - 2);
          if (check_idx < src_lines.size()) {
            while (check_idx > 0 && llvm::StringRef(src_lines[check_idx]).trim().empty()) {
              --check_idx;
            }
            llvm::StringRef cand = llvm::StringRef(src_lines[check_idx]).trim();
            if (cand.starts_with("#pragma") && cand.contains("omp ")) {
              std::string* owned = nullptr;
              LLVM_GEN_BE_NEW(owned, std::string(cand.str()));
              be_state->omp_scanned_pragma_strings.emplace_back(owned);
              be_state->omp_pragmas[cur] = owned->c_str();
              cur->has_associated_pragma = TRUE;
            }
          }
        }
      }
    }

    switch (cur->kind) {
      case stmk_block:
        LLVM_GEN_BE_CHECK(scan_omp_pragmas_in_stmt_tree(cur->variant.block.statements, claimed_lines));
        break;
      case stmk_for:
        LLVM_GEN_BE_CHECK(scan_omp_pragmas_in_stmt_tree(cur->variant.for_loop.statement, claimed_lines));
        break;
      case stmk_while:
      case stmk_end_test_while:
        LLVM_GEN_BE_CHECK(scan_omp_pragmas_in_stmt_tree(cur->variant.loop_statement, claimed_lines));
        break;
      case stmk_if:
        LLVM_GEN_BE_CHECK(scan_omp_pragmas_in_stmt_tree(cur->variant.if_stmt.then_statement, claimed_lines));
        LLVM_GEN_BE_CHECK(scan_omp_pragmas_in_stmt_tree(cur->variant.if_stmt.else_statement, claimed_lines));
        break;
      case stmk_switch:
        LLVM_GEN_BE_CHECK(scan_omp_pragmas_in_stmt_tree(cur->variant.switch_stmt.body_statement, claimed_lines));
        break;
      default:
        break;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Extracts a string argument from an EDG attribute node (`aak_constant`, `aak_token`, or `aak_raw_token`).
 * @details Strips surrounding double quotes when the argument is a token string.
 * @param[in] attr Pointer to the EDG attribute node.
 * @param[out] out_str Receives the extracted attribute string (or empty string if none).
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t extract_attr_string(
    an_attribute_ptr attr,
    std::string* out_str) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  out_str->clear();
  if (attr->arguments) {
    if (attr->arguments->kind == aak_constant) {
      a_constant_ptr const_ptr = attr->arguments->variant.constant;
      if (const_ptr && const_ptr->kind == ck_string) {
        *out_str = std::string(const_ptr->variant.string.value, static_cast<size_t>(const_ptr->variant.string.length - 1));
      }
    } else if ((attr->arguments->kind == aak_token || attr->arguments->kind == aak_raw_token) &&
               attr->arguments->variant.token) {
      *out_str = attr->arguments->variant.token;
      if (llvm::StringRef(*out_str).starts_with("\"")) {
        *out_str = out_str->substr(1, out_str->size() - 2);
      }
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Applies EDG attributes to an LLVM GlobalObject.
 * @details Implements or declares `apply_llvm_attributes` for the EDG LLVM backend.
 * @param[in] global The LLVM GlobalObject to apply attributes to.
 * @param[in] attributes The EDG attributes list.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
static llvm_gen_be_error_t apply_llvm_attributes(llvm::GlobalObject* global, an_attribute_ptr attributes) noexcept {
    for (an_attribute_ptr attr = attributes; attr != nullptr; attr = attr->next) {
        if (attr->kind == ak_section) {
            std::string sec_name;
            LLVM_GEN_BE_CHECK(extract_attr_string(attr, &sec_name));
            if (!sec_name.empty()) {
                global->setSection(sec_name);
            }
        }
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
        else if (attr->kind == ak_visibility) {
            std::string vis_str;
            LLVM_GEN_BE_CHECK(extract_attr_string(attr, &vis_str));
            if (vis_str == "hidden") {
                global->setVisibility(llvm::GlobalValue::HiddenVisibility);
            } else if (vis_str == "protected") {
                global->setVisibility(llvm::GlobalValue::ProtectedVisibility);
            } else if (vis_str == "default") {
                global->setVisibility(llvm::GlobalValue::DefaultVisibility);
                be_state->explicit_default_vis.insert(global);
            }
        }
#endif
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
        else if (attr->kind == ak_dllexport) {
            global->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);
        } else if (attr->kind == ak_dllimport) {
            global->setDLLStorageClass(llvm::GlobalValue::DLLImportStorageClass);
        } else if (attr->kind == ak_cold) {
            if (llvm::Function* F = llvm::dyn_cast<llvm::Function>(global)) F->addFnAttr(llvm::Attribute::Cold);
        } else if (attr->kind == ak_hot) {
            if (llvm::Function* F = llvm::dyn_cast<llvm::Function>(global)) F->addFnAttr(llvm::Attribute::Hot);
        } else if (attr->kind == ak_naked) {
            if (llvm::Function* F = llvm::dyn_cast<llvm::Function>(global)) F->addFnAttr(llvm::Attribute::Naked);
        } else if (attr->kind == ak_used) {
            llvm::Module* M = global->getParent();
            llvm::Type* ptr_ty = llvm::PointerType::getUnqual(M->getContext());
            llvm::GlobalVariable* used_gv = M->getGlobalVariable("llvm.used");
            std::vector<llvm::Constant*> elems;
            if (used_gv) {
                llvm::ConstantArray* old_arr = llvm::cast<llvm::ConstantArray>(used_gv->getInitializer());
                for (unsigned i = 0; i < old_arr->getNumOperands(); ++i) {
                    elems.push_back(llvm::cast<llvm::Constant>(old_arr->getOperand(i)));
                }
                used_gv->eraseFromParent();
            }
            elems.push_back(global);
            llvm::ArrayType* arr_ty = llvm::ArrayType::get(ptr_ty, elems.size());
            LLVM_GEN_BE_CHECK(llvm_create_global_variable(M, arr_ty, false, llvm::GlobalValue::AppendingLinkage, llvm::ConstantArray::get(arr_ty, elems), "llvm.used", &used_gv));
            used_gv->setSection("llvm.metadata");
        } else if (attr->kind == ak_alias) {
            std::string target_name;
            LLVM_GEN_BE_CHECK(extract_attr_string(attr, &target_name));
            if (!target_name.empty()) {
                llvm::Module* M = global->getParent();
                llvm::GlobalValue* aliasee = M->getNamedValue(target_name);
                if (aliasee) {
                    std::string orig_name = global->getName().str();
                    llvm::Type* val_ty = global->getValueType();
                    unsigned addr_space = global->getAddressSpace();
                    llvm::GlobalValue::LinkageTypes link_ty = global->getLinkage();
                    global->setName(orig_name + ".decl");
                    llvm::GlobalAlias* ga = llvm::GlobalAlias::create(
                        val_ty, addr_space, link_ty, orig_name, aliasee, M);
                    global->replaceAllUsesWith(ga);
                }
            }
        } else if (attr->kind == ak_ifunc) {
            std::string resolver_name;
            LLVM_GEN_BE_CHECK(extract_attr_string(attr, &resolver_name));
            if (!resolver_name.empty()) {
                llvm::Module* M = global->getParent();
                llvm::Function* resolver = M->getFunction(resolver_name);
                if (resolver) {
                    std::string orig_name = global->getName().str();
                    llvm::Type* val_ty = global->getValueType();
                    unsigned addr_space = global->getAddressSpace();
                    llvm::GlobalValue::LinkageTypes link_ty = global->getLinkage();
                    global->setName(orig_name + ".decl");
                    llvm::GlobalIFunc* gi = llvm::GlobalIFunc::create(
                        val_ty, addr_space, link_ty, orig_name, resolver, M);
                    global->replaceAllUsesWith(gi);
                }
            }
        } else if (llvm::Function* F = llvm::dyn_cast<llvm::Function>(global)) {
            if (attr->kind == ak_const) {
                F->setDoesNotAccessMemory();
            } else if (attr->kind == ak_pure) {
                F->setOnlyReadsMemory();
            } else if (attr->kind == ak_noreturn) {
                F->addFnAttr(llvm::Attribute::NoReturn);
            } else if (attr->kind == ak_nothrow) {
                F->addFnAttr(llvm::Attribute::NoUnwind);
            } else if (attr->kind == ak_warn_unused_result || attr->kind == ak_nodiscard) {
                F->addFnAttr("warn-unused-result");
            } else if (attr->kind == ak_alloc_size) {
                unsigned alloc_arg_idx = (F->arg_size() > 1) ? static_cast<unsigned>(F->arg_size() - 1) : 0u;
                F->addFnAttr(llvm::Attribute::getWithAllocSizeArgs(F->getContext(), alloc_arg_idx, std::nullopt));
#if MICROSOFT_EXTENSIONS_ALLOWED
            } else if (attr->kind == ak_no_sanitize_address) {
                F->removeFnAttr(llvm::Attribute::SanitizeAddress);
                F->addFnAttr("no_sanitize_address");
#endif
            } else if (attr->kind == ak_unrecognized) {
                std::string attr_tok;
                LLVM_GEN_BE_CHECK(extract_attr_string(attr, &attr_tok));
                if (attr_tok == "returns_twice") {
                    F->addFnAttr(llvm::Attribute::ReturnsTwice);
                } else if (attr_tok == "alloc_align") {
                    F->addFnAttr(llvm::Attribute::AllocAlign);
                } else if (attr_tok == "no_sanitize" || attr_tok == "no_sanitize_address" ||
                           attr_tok == "no_sanitize_thread" || attr_tok == "no_sanitize_memory" ||
                           attr_tok == "no_sanitize_undefined") {
                    F->removeFnAttr(llvm::Attribute::SanitizeAddress);
                    F->removeFnAttr(llvm::Attribute::SanitizeThread);
                    F->removeFnAttr(llvm::Attribute::SanitizeMemory);
                    F->removeFnAttr(llvm::Attribute::SanitizeHWAddress);
                    F->removeFnAttr("ubsan");
                    F->addFnAttr(attr_tok);
                } else if (attr_tok == "regcall") {
                    F->setCallingConv(llvm::CallingConv::X86_RegCall);
                } else if (attr_tok == "preserve_most") {
                    F->setCallingConv(llvm::CallingConv::PreserveMost);
                } else if (attr_tok == "preserve_all") {
                    F->setCallingConv(llvm::CallingConv::PreserveAll);
                } else if (attr_tok == "aarch64_vector_pcs") {
                    F->setCallingConv(llvm::CallingConv::AArch64_VectorCall);
                } else if (attr_tok == "aarch64_sve_pcs") {
                    F->setCallingConv(llvm::CallingConv::AArch64_SVE_VectorCall);
                } else if (attr_tok == "gpu_kernel" || attr_tok == "global") {
                    llvm::Module* M = F->getParent();
                    llvm::Triple tr = llvm::Triple(M->getTargetTriple());
                    if (tr.isNVPTX()) {
                        F->setCallingConv(llvm::CallingConv::PTX_Kernel);
                        llvm::NamedMDNode* nvvm_md = M->getOrInsertNamedMetadata("nvvm.annotations");
                        llvm::LLVMContext& Ctx = M->getContext();
                        nvvm_md->addOperand(llvm::MDNode::get(Ctx, {
                            llvm::ValueAsMetadata::get(F),
                            llvm::MDString::get(Ctx, "kernel"),
                            llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 1))
                        }));
                    } else if (tr.isAMDGPU()) {
                        F->setCallingConv(llvm::CallingConv::AMDGPU_KERNEL);
                    } else {
                        F->setCallingConv(llvm::CallingConv::SPIR_KERNEL);
                    }
                }
            }
        }
        #endif
    }
    return llvm_gen_be_error_t::ok;
}

#if GNU_EXTENSIONS_ALLOWED && GNU_VISIBILITY_ATTRIBUTE_ALLOWED
/**
 * @brief Applies EDG's resolved `an_ELF_visibility_kind` to an LLVM `GlobalValue`.
 * @details Records `evk_default` symbols in `be_state->explicit_default_vis` so `-fvisibility=hidden`
 *          does not override explicit `#pragma GCC visibility push(default)` or `FMT_API` declarations.
 * @param[in,out] global The target `llvm::GlobalValue` to update.
 * @param[in] evk The EDG-computed ELF visibility kind.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t apply_elf_visibility(
    llvm::GlobalValue* global,
    an_ELF_visibility_kind evk) noexcept {
  if (evk == evk_default) {
    global->setVisibility(llvm::GlobalValue::DefaultVisibility);
    be_state->explicit_default_vis.insert(global);
  } else if (evk == evk_hidden || evk == evk_internal) {
    global->setVisibility(llvm::GlobalValue::HiddenVisibility);
  } else if (evk == evk_protected) {
    global->setVisibility(llvm::GlobalValue::ProtectedVisibility);
  }
  return llvm_gen_be_error_t::ok;
}
#endif

/**
 * @brief Encodes an EDG parameter or template argument type into its Itanium C++ ABI type code.
 * @details Handles `void` (`v`), `bool` (`b`), `wchar_t` (`w`), `char8_t` (`Du`), `char16_t` (`Ds`),
 *          `char32_t` (`Di`), integer kinds (`c`, `a`, `h`, `s`, `t`, `i`, `j`, `l`, `m`, `x`, `y`, `n`, `o`),
 *          floating-point kinds (`f`, `d`, `e`, `g`, `DF16_`, `DF16b`), `nullptr_t` (`Dn`), pointers (`P`),
 *          lvalue/rvalue references (`R`, `O`), arrays (`A<N>_`), routines (`F...E`), pointer-to-member (`M`),
 *          vectors (`Dv<N>_`), complex (`C`), cv-qualifiers (`K`, `V`, `r`), and named user types (`<len><name>`).
 * @param[in] ty The EDG type to encode.
 * @param[out] out_code Appends the encoded Itanium type string to `*out_code`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t encode_itanium_param_type(
    a_type_ptr ty,
    std::string* out_code) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  if (!ty) {
    *out_code += "v";
    return llvm_gen_be_error_t::ok;
  }
  a_type_qualifier_set quals = f_get_type_qualifiers(ty, FALSE);
  if ((quals & TQ_RESTRICT) != 0) *out_code += "r";
  if ((quals & TQ_VOLATILE) != 0) *out_code += "V";
  if ((quals & TQ_CONST) != 0) *out_code += "K";

  a_type_ptr ut = skip_typerefs(ty);
  switch (ut->kind) {
    case tk_void:
      *out_code += "v";
      break;
    case tk_nullptr:
      *out_code += "Dn";
      break;
    case tk_integer:
      if (ut->variant.integer.bool_type) {
        *out_code += "b";
      } else if (ut->variant.integer.wchar_t_type) {
        *out_code += "w";
      } else if (ut->variant.integer.char8_t_type) {
        *out_code += "Du";
      } else if (ut->variant.integer.char16_t_type) {
        *out_code += "Ds";
      } else if (ut->variant.integer.char32_t_type) {
        *out_code += "Di";
      } else if (ut->size == 16) {
        *out_code += ut->variant.integer.explicitly_signed ? "n" : "o";
      } else {
        switch (ut->variant.integer.int_kind) {
          case ik_char: *out_code += "c"; break;
          case ik_signed_char: *out_code += "a"; break;
          case ik_unsigned_char: *out_code += "h"; break;
          case ik_short: *out_code += "s"; break;
          case ik_unsigned_short: *out_code += "t"; break;
          case ik_int: *out_code += "i"; break;
          case ik_unsigned_int: *out_code += "j"; break;
          case ik_long: *out_code += "l"; break;
          case ik_unsigned_long: *out_code += "m"; break;
          case ik_long_long: *out_code += "x"; break;
          case ik_unsigned_long_long: *out_code += "y"; break;
          default: *out_code += "i"; break;
        }
      }
      break;
    case tk_float:
      if (ut->variant.float_kind == fk_float) *out_code += "f";
      else if (ut->variant.float_kind == fk_long_double) *out_code += "e";
      else if (ut->variant.float_kind == fk_float128) *out_code += "g";
      else if (ut->variant.float_kind == fk_float16 || ut->variant.float_kind == fk_fp16) *out_code += "DF16_";
      else if (ut->variant.float_kind == fk_std_bfloat16) *out_code += "DF16b";
      else *out_code += "d";
      break;
    case tk_complex: {
      *out_code += "C";
      a_type elem_fp = {};
      elem_fp.kind = tk_float;
      elem_fp.variant.float_kind = ut->variant.float_kind;
      LLVM_GEN_BE_CHECK(encode_itanium_param_type(&elem_fp, out_code));
      break;
    }
    case tk_pointer:
      *out_code += ut->variant.pointer.is_reference
                       ? (ut->variant.pointer.is_rvalue_reference ? "O" : "R")
                       : "P";
      LLVM_GEN_BE_CHECK(encode_itanium_param_type(ut->variant.pointer.type, out_code));
      break;
    case tk_array:
      *out_code += "A" + std::to_string(static_cast<uint64_t>(ut->variant.array.variant.number_of_elements)) + "_";
      LLVM_GEN_BE_CHECK(encode_itanium_param_type(ut->variant.array.element_type, out_code));
      break;
    case tk_routine: {
      *out_code += "F";
      LLVM_GEN_BE_CHECK(encode_itanium_param_type(ut->variant.routine.return_type, out_code));
      a_routine_type_supplement_ptr rsupp = ut->variant.routine.extra_info;
      if (!rsupp) {
        *out_code += "v";
      } else {
        for (a_param_type_ptr pt = rsupp->param_type_list; pt != nullptr; pt = pt->next) {
          LLVM_GEN_BE_CHECK(encode_itanium_param_type(pt->type, out_code));
        }
      }
      *out_code += "E";
      break;
    }
    case tk_ptr_to_member:
      *out_code += "M";
      LLVM_GEN_BE_CHECK(encode_itanium_param_type(ut->variant.ptr_to_member.class_of_which_a_member, out_code));
      LLVM_GEN_BE_CHECK(encode_itanium_param_type(ut->variant.ptr_to_member.type, out_code));
      break;
#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
    case tk_vector: {
      a_type_ptr elem_ty = ut->variant.vector.element_type;
      a_type_ptr raw_elem = elem_ty ? skip_typerefs(elem_ty) : nullptr;
      uint64_t elem_sz = (raw_elem && raw_elem->size > 0) ? static_cast<uint64_t>(raw_elem->size) : 4ULL;
      uint64_t num_elems = (ut->size > 0) ? (static_cast<uint64_t>(ut->size) / elem_sz) : 4ULL;
      *out_code += "Dv" + std::to_string(num_elems) + "_";
      LLVM_GEN_BE_CHECK(encode_itanium_param_type(elem_ty, out_code));
      break;
    }
#endif
    default: {
      const char* tname = ut->source_corresp.name ? ut->source_corresp.name : "T";
      std::string ts = std::string(tname);
      *out_code += std::to_string(ts.size()) + ts;
      break;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Encodes an overloaded C++ operator name or identifier into its Itanium ABI `<unqualified-name>`.
 * @details Maps all C++ arithmetic, bitwise, relational, logical, compound-assignment, memory
 *          allocation/deallocation, subscript/call/member-access, and spaceship operators
 *          to their Itanium ABI two-letter codes, or length-prefixes `<len><id>`.
 * @param[in] base_name Unqualified C++ entity or operator name.
 * @param[out] out_encoded Appends the Itanium unqualified-name encoding to `*out_encoded`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t encode_itanium_operator_or_name(
    llvm::StringRef base_name,
    std::string* out_encoded) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  if (base_name == "operator+") *out_encoded += "pl";
  else if (base_name == "operator-") *out_encoded += "mi";
  else if (base_name == "operator*") *out_encoded += "ml";
  else if (base_name == "operator/") *out_encoded += "dv";
  else if (base_name == "operator%") *out_encoded += "rm";
  else if (base_name == "operator^") *out_encoded += "eo";
  else if (base_name == "operator&") *out_encoded += "an";
  else if (base_name == "operator|") *out_encoded += "or";
  else if (base_name == "operator~") *out_encoded += "co";
  else if (base_name == "operator!") *out_encoded += "nt";
  else if (base_name == "operator=") *out_encoded += "aS";
  else if (base_name == "operator<") *out_encoded += "lt";
  else if (base_name == "operator>") *out_encoded += "gt";
  else if (base_name == "operator+=") *out_encoded += "pL";
  else if (base_name == "operator-=") *out_encoded += "mI";
  else if (base_name == "operator*=") *out_encoded += "mL";
  else if (base_name == "operator/=") *out_encoded += "dV";
  else if (base_name == "operator%=") *out_encoded += "rM";
  else if (base_name == "operator^=") *out_encoded += "eO";
  else if (base_name == "operator&=") *out_encoded += "aN";
  else if (base_name == "operator|=") *out_encoded += "oR";
  else if (base_name == "operator<<") *out_encoded += "ls";
  else if (base_name == "operator>>") *out_encoded += "rs";
  else if (base_name == "operator<<=") *out_encoded += "lS";
  else if (base_name == "operator>>=") *out_encoded += "rS";
  else if (base_name == "operator==") *out_encoded += "eq";
  else if (base_name == "operator!=") *out_encoded += "ne";
  else if (base_name == "operator<=") *out_encoded += "le";
  else if (base_name == "operator>=") *out_encoded += "ge";
  else if (base_name == "operator<=>") *out_encoded += "ss";
  else if (base_name == "operator&&") *out_encoded += "aa";
  else if (base_name == "operator||") *out_encoded += "oo";
  else if (base_name == "operator++") *out_encoded += "pp";
  else if (base_name == "operator--") *out_encoded += "mm";
  else if (base_name == "operator,") *out_encoded += "cm";
  else if (base_name == "operator->*") *out_encoded += "pm";
  else if (base_name == "operator->") *out_encoded += "pt";
  else if (base_name == "operator()") *out_encoded += "cl";
  else if (base_name == "operator[]") *out_encoded += "ix";
  else if (base_name == "operator?") *out_encoded += "qu";
  else if (base_name == "operator+unary") *out_encoded += "ps";
  else if (base_name == "operator-unary") *out_encoded += "ng";
  else if (base_name == "operator&unary") *out_encoded += "ad";
  else if (base_name == "operator*unary") *out_encoded += "de";
  else if (base_name == "operator new") *out_encoded += "nw";
  else if (base_name == "operator new[]") *out_encoded += "na";
  else if (base_name == "operator delete") *out_encoded += "dl";
  else if (base_name == "operator delete[]") *out_encoded += "da";
  else if (base_name == "C1" || base_name == "C2" || base_name == "C3" ||
           base_name == "D0" || base_name == "D1" || base_name == "D2") {
    *out_encoded += base_name.str();
  } else if (base_name.starts_with("operator\"\"")) {
    llvm::StringRef lit_id = base_name.drop_front(10).ltrim();
    *out_encoded += "li" + std::to_string(lit_id.size()) + lit_id.str();
  } else if (base_name.starts_with("operator ")) {
    llvm::StringRef conv_ty = base_name.drop_front(9).ltrim();
    if (conv_ty == "int") *out_encoded += "cvi";
    else if (conv_ty == "bool") *out_encoded += "cvb";
    else *out_encoded += "cv" + std::to_string(conv_ty.size()) + conv_ty.str();
  } else {
    *out_encoded += std::to_string(base_name.size()) + base_name.str();
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Encodes an EDG type into its Microsoft Visual C++ (MSVC) ABI type code.
 * @details Maps `void` (`X`), `bool` (`_N`), `char` (`D`), `signed char` (`C`), `unsigned char` (`E`),
 *          `short` (`F`), `unsigned short` (`G`), `int` (`H`), `unsigned int` (`I`), `long` (`J`),
 *          `unsigned long` (`K`), `__int64` (`_J`), `unsigned __int64` (`_K`), `float` (`M`),
 *          `double` (`N`), pointers (`PEAX` / `PEAV<Class>@@` / `PEAU<Struct>@@`),
 *          lvalue references (`AEAH` / `AEAV<Class>@@`), rvalue references (`$$QEAV<Class>@@`),
 *          and user classes/structs (`V<Class>@@` / `U<Struct>@@`).
 * @param[in] ty The EDG type to encode.
 * @param[out] out_code Appends the MSVC type code to `*out_code`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t encode_msvc_type_code(
    a_type_ptr ty,
    std::string* out_code) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  if (!ty) {
    *out_code += "X";
    return llvm_gen_be_error_t::ok;
  }
  a_type_ptr ut = skip_typerefs(ty);
  switch (ut->kind) {
    case tk_void: *out_code += "X"; break;
    case tk_integer:
      if (ut->variant.integer.bool_type) *out_code += "_N";
      else if (ut->variant.integer.int_kind == ik_char) *out_code += "D";
      else if (ut->variant.integer.int_kind == ik_signed_char) *out_code += "C";
      else if (ut->variant.integer.int_kind == ik_unsigned_char) *out_code += "E";
      else if (ut->variant.integer.int_kind == ik_short) *out_code += "F";
      else if (ut->variant.integer.int_kind == ik_unsigned_short) *out_code += "G";
      else if (ut->variant.integer.int_kind == ik_unsigned_int) *out_code += "I";
      else if (ut->variant.integer.int_kind == ik_long) *out_code += "J";
      else if (ut->variant.integer.int_kind == ik_unsigned_long) *out_code += "K";
      else if (ut->variant.integer.int_kind == ik_long_long) *out_code += "_J";
      else if (ut->variant.integer.int_kind == ik_unsigned_long_long) *out_code += "_K";
      else *out_code += "H";
      break;
    case tk_float:
      *out_code += (ut->variant.float_kind == fk_float) ? "M" : "N";
      break;
    case tk_pointer:
      if (ut->variant.pointer.is_reference) {
        *out_code += ut->variant.pointer.is_rvalue_reference ? "$$QEA" : "AEA";
      } else {
        *out_code += "PEA";
      }
      LLVM_GEN_BE_CHECK(encode_msvc_type_code(ut->variant.pointer.type, out_code));
      break;
    default: {
      const char* cname = ut->source_corresp.name ? ut->source_corresp.name : "T";
      const char* tag_prefix = (ut->kind == tk_struct) ? "U" : "V";
      *out_code += std::string(tag_prefix) + cname + "@@";
    } break;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Mangles an unlowered C++ entity (variable, function, method, or special member) under Itanium or MSVC ABI.
 * @details Encodes nested namespace/class/function scopes (`_ZN...E`, `_ZZ...E`), cv-qualifiers,
 *          reference qualifiers, template arguments (`I...E` / `?$Name@...`), standard substitutions
 *          (`St`, `Sa`, `Sb`, `Ss`, `Si`, `So`, `Sd`), template parameter codes (`T_`, `T0_`),
 *          special entities (`C1`/`C2`/`C3`, `D0`/`D1`/`D2`, `_ZGV`, `_ZGR`), and MSVC access/calling
 *          convention/operator codes (`??0`, `??1`, `??2`, `??3`, `??_U`, `??_V`, `??4`, `??8`, `??_G`, `??_E`).
 * @param[in] scp Pointer to the entity's `a_source_correspondence`.
 * @param[in] entity_type Pointer to the entity's EDG type (`tk_routine` for functions, or data type).
 * @param[in] is_msvc `true` to synthesize Microsoft Visual C++ (`?name@...`) mangling, `false` for Itanium (`_Z...`).
 * @param[out] out_mangled Receives the synthesized mangled symbol name.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if required pointers are null.
 */
llvm_gen_be_error_t llvm_mangle_unlowered_entity(
    a_source_correspondence* scp,
    a_type_ptr entity_type,
    bool is_msvc,
    std::string* out_mangled) noexcept {
  if (!scp || !out_mangled) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  const char* raw_name = scp->name ? scp->name : "anon";
  if (scp->name_has_been_mangled ||
      llvm::StringRef(raw_name).starts_with("_Z") ||
      raw_name[0] == '?') {
    *out_mangled = raw_name;
    return llvm_gen_be_error_t::ok;
  }

  std::vector<std::string> scopes;
  a_scope_ptr pscope = scp->parent_scope;
  bool is_class_member = false;
  bool is_local_scope = false;
  std::string enclosing_fn_name;
  while (pscope && (pscope->kind == sck_namespace || pscope->kind == sck_class_struct_union ||
                    pscope->kind == sck_function)) {
    if (pscope->kind == sck_namespace) {
      a_namespace_ptr ns = pscope->variant.assoc_namespace;
      const char* ns_name = ns ? ns->source_corresp.name : nullptr;
      scopes.push_back(ns_name ? std::string(ns_name) : (is_msvc ? "?A0x1" : "_GLOBAL__N_1"));
      pscope = ns ? ns->source_corresp.parent_scope : pscope->parent;
    } else if (pscope->kind == sck_class_struct_union) {
      is_class_member = true;
      a_type_ptr tp = pscope->variant.assoc_type;
      const char* tp_name = tp ? tp->source_corresp.name : "AnonClass";
      scopes.push_back(tp_name);
      pscope = tp ? tp->source_corresp.parent_scope : pscope->parent;
    } else {
      is_local_scope = true;
      a_routine_ptr fn_r = pscope->variant.routine.ptr;
      enclosing_fn_name = fn_r ? fn_r->source_corresp.name : "local_fn";
      pscope = pscope->parent;
    }
  }

  bool is_func = (entity_type && skip_typerefs(entity_type)->kind == tk_routine);
  llvm::StringRef rname_ref = llvm::StringRef(raw_name);
  if (is_msvc) {
    std::string msvc_name = "?";
    size_t lt_m = rname_ref.find('<');
    size_t gt_m = rname_ref.rfind('>');
    if (rname_ref == "C1" || rname_ref == "C2") msvc_name += "?0";
    else if (rname_ref == "D1" || rname_ref == "D2" || rname_ref.starts_with("~")) msvc_name += "?1";
    else if (rname_ref == "D0") msvc_name += "?_G";
    else if (rname_ref == "D_vec") msvc_name += "?_E";
    else if (rname_ref == "operator new") msvc_name += "?2";
    else if (rname_ref == "operator delete") msvc_name += "?3";
    else if (rname_ref == "operator new[]") msvc_name += "?_U";
    else if (rname_ref == "operator delete[]") msvc_name += "?_V";
    else if (rname_ref == "operator=") msvc_name += "?4";
    else if (rname_ref == "operator==") msvc_name += "?8";
    else if (lt_m != std::string::npos) {
      llvm::StringRef tpl_base = rname_ref.substr(0, lt_m);
      llvm::StringRef tpl_arg = rname_ref.substr(lt_m + 1, gt_m - lt_m - 1);
      msvc_name += "?$" + tpl_base.str() + "@";
      msvc_name += (tpl_arg == "int") ? "H" : ("V" + tpl_arg.str() + "@@");
      msvc_name += "@";
    } else {
      msvc_name += raw_name;
      msvc_name += "@";
    }
    for (const std::string& sc : scopes) {
      msvc_name += sc + "@";
    }
    msvc_name += "@";
    if (is_func) {
      a_type_ptr rty = skip_typerefs(entity_type);
      a_routine_type_supplement_ptr rsupp = rty->variant.routine.extra_info;
      char cc_code = 'A';
      if (rsupp) {
        if (rsupp->calling_convention == cc_stdcall) cc_code = 'G';
        else if (rsupp->calling_convention == cc_thiscall) cc_code = 'E';
        else if (rsupp->calling_convention == cc_fastcall) cc_code = 'I';
        else if (rsupp->calling_convention == cc_vectorcall) cc_code = 'Q';
      }
      if (is_class_member) {
        char access_code = 'Q';
        if (scp->access == as_private) access_code = 'A';
        else if (scp->access == as_protected) access_code = 'I';
        bool is_const_mem = (rsupp && (rsupp->qualifiers & TQ_CONST) != 0);
        msvc_name += access_code;
        msvc_name += "E";
        msvc_name += is_const_mem ? "B" : "A";
        msvc_name += cc_code;
      } else {
        msvc_name += "Y";
        msvc_name += cc_code;
      }
      LLVM_GEN_BE_CHECK(encode_msvc_type_code(rty->variant.routine.return_type, &msvc_name));
      if (!rsupp || !rsupp->param_type_list) {
        msvc_name += "XZ";
      } else {
        std::vector<std::string> msvc_backrefs;
        for (a_param_type_ptr pt = rsupp->param_type_list; pt != nullptr; pt = pt->next) {
          std::string pcode;
          LLVM_GEN_BE_CHECK(encode_msvc_type_code(pt->type, &pcode));
          bool found_br = false;
          if (pcode.size() > 1) {
            for (size_t bi = 0; bi < msvc_backrefs.size(); ++bi) {
              if (msvc_backrefs[bi] == pcode) {
                msvc_name += std::to_string(bi);
                found_br = true;
                break;
              }
            }
            if (!found_br) msvc_backrefs.push_back(pcode);
          }
          if (!found_br) msvc_name += pcode;
        }
        msvc_name += "@Z";
      }
    } else {
      msvc_name += is_class_member ? "2" : "3";
      LLVM_GEN_BE_CHECK(encode_msvc_type_code(entity_type, &msvc_name));
      msvc_name += "A";
    }
    *out_mangled = msvc_name;
    return llvm_gen_be_error_t::ok;
  }

  std::string mangled = "_Z";
  if (is_local_scope) {
    mangled += "Z" + std::to_string(enclosing_fn_name.size()) + enclosing_fn_name + "vE";
  }
  bool has_scopes = !scopes.empty();
  a_routine_type_supplement_ptr fn_supp =
      is_func ? skip_typerefs(entity_type)->variant.routine.extra_info : nullptr;
  if (has_scopes) {
    mangled += "N";
    if (fn_supp) {
      if ((fn_supp->qualifiers & TQ_VOLATILE) != 0) mangled += "V";
      if ((fn_supp->qualifiers & TQ_CONST) != 0) mangled += "K";
      if (fn_supp->ref_qualifiers == rqk_lvalue) mangled += "R";
      else if (fn_supp->ref_qualifiers == rqk_rvalue) mangled += "O";
    }
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      if (*it == "std") mangled += "St";
      else if (*it == "std::allocator") mangled += "Sa";
      else if (*it == "std::basic_string") mangled += "Sb";
      else if (*it == "std::string") mangled += "Ss";
      else if (*it == "std::istream") mangled += "Si";
      else if (*it == "std::ostream") mangled += "So";
      else if (*it == "std::iostream") mangled += "Sd";
      else mangled += std::to_string(it->size()) + *it;
    }
  }
  std::string ename = std::string(raw_name);
  size_t lt_pos = ename.find('<');
  size_t gt_pos = ename.rfind('>');
  if (lt_pos != std::string::npos && gt_pos != std::string::npos && lt_pos > 0 && ename != "operator<=>") {
    std::string base_part = ename.substr(0, lt_pos);
    std::string targs_str = ename.substr(lt_pos + 1, gt_pos - lt_pos - 1);
    LLVM_GEN_BE_CHECK(encode_itanium_operator_or_name(base_part, &mangled));
    mangled += "I";
    size_t start = 0;
    for (;;) {
      size_t comma = targs_str.find(',', start);
      std::string one_arg = (comma == std::string::npos)
                                ? targs_str.substr(start)
                                : targs_str.substr(start, comma - start);
      while (one_arg.front() == ' ') one_arg.erase(one_arg.begin());
      if ((one_arg[0] >= '0' && one_arg[0] <= '9') || one_arg[0] == '-') {
        if (one_arg[0] == '-') {
          mangled += "Lin" + one_arg.substr(1) + "E";
        } else {
          mangled += "Li" + one_arg + "E";
        }
      } else if (one_arg == "int") {
        mangled += "i";
      } else if (one_arg == "T_" || one_arg == "T0_") {
        mangled += one_arg;
      } else {
        mangled += std::to_string(one_arg.size()) + one_arg;
      }
      if (comma == std::string::npos) break;
      start = comma + 1;
    }
    mangled += "E";
  } else {
    LLVM_GEN_BE_CHECK(encode_itanium_operator_or_name(ename, &mangled));
  }
  if (has_scopes) {
    mangled += "E";
  }

  if (is_func) {
    a_type_ptr rty = skip_typerefs(entity_type);
    a_routine_type_supplement_ptr rsupp = rty->variant.routine.extra_info;
    if (!rsupp || !rsupp->param_type_list) {
      mangled += "v";
    } else {
      std::vector<std::string> seen_types;
      for (a_param_type_ptr pt = rsupp->param_type_list; pt != nullptr; pt = pt->next) {
        std::string tcode;
        LLVM_GEN_BE_CHECK(encode_itanium_param_type(pt->type, &tcode));
        bool substituted = false;
        if (tcode.size() > 1) {
          for (size_t s_idx = 0; s_idx < seen_types.size(); ++s_idx) {
            if (seen_types[s_idx] == tcode) {
              mangled += (s_idx == 0) ? "S_" : ("S" + std::to_string(s_idx - 1) + "_");
              substituted = true;
              break;
            }
          }
          if (!substituted) {
            seen_types.push_back(tcode);
          }
        }
        if (!substituted) {
          mangled += tcode;
        }
      }
    }
  }

  *out_mangled = mangled;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resolves the mangled or external linkage symbol name for an EDG variable.
 * @details Preserves already-mangled names (`name_has_been_mangled` or `_Z`/`?` prefixes) and `extern "C"`
 *          or file/local-scope names, and synthesizes Itanium `_ZN...E` nested names when `parent_scope`
 *          is a namespace (`sck_namespace`) or class (`sck_class_struct_union`).
 * @param[in] var Pointer to the EDG variable.
 * @param[out] out_name Receives the resolved linkage symbol name.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t llvm_get_mangled_variable_name(
    a_variable_ptr var,
    std::string* out_name) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  const char* raw_name = (var->source_corresp.name[0] != '\0')
                             ? var->source_corresp.name
                             : "anon_var";
  a_scope_ptr pscope = var->source_corresp.parent_scope;
  if (!pscope ||
      var->source_corresp.name_has_been_mangled ||
      var->source_corresp.name_linkage == nlk_external ||
      (raw_name[0] == '_' && raw_name[1] == 'Z') ||
      raw_name[0] == '?' ||
      (pscope->kind != sck_namespace && pscope->kind != sck_class_struct_union)) {
    *out_name = raw_name;
    return llvm_gen_be_error_t::ok;
  }
  std::vector<std::string> components;
  while (pscope->kind == sck_namespace || pscope->kind == sck_class_struct_union) {
    if (pscope->kind == sck_namespace) {
      a_namespace_ptr ns = pscope->variant.assoc_namespace;
      const char* ns_name = (ns && ns->source_corresp.name[0] != '\0')
                                ? ns->source_corresp.name
                                : nullptr;
      if (ns_name) {
        std::string s = std::string(ns_name);
        components.push_back(std::to_string(s.size()) + s);
      } else {
        components.push_back("12_GLOBAL__N_1");
      }
      pscope = ns ? ns->source_corresp.parent_scope : pscope->parent;
    } else {
      a_type_ptr tp = pscope->variant.assoc_type;
      const char* tp_name = (tp && tp->source_corresp.name[0] != '\0')
                                ? tp->source_corresp.name
                                : "AnonClass";
      std::string s = std::string(tp_name);
      components.push_back(std::to_string(s.size()) + s);
      pscope = tp ? tp->source_corresp.parent_scope : pscope->parent;
    }
  }
  std::string mangled = "_ZN";
  for (auto it = components.rbegin(); it != components.rend(); ++it) {
    mangled += *it;
  }
  std::string vname = std::string(raw_name);
  mangled += std::to_string(vname.size()) + vname + "E";
  *out_name = mangled;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a single EDG global variable into the LLVM Module.
 * @details Implements or declares `llvm_lower_global_variable` for the EDG LLVM backend.
 * @param[in] var The EDG variable.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_lower_global_variable(a_variable_ptr var) noexcept {
    if (!var || !var->source_corresp.name ||
        ignore_variable_in_back_end(var) ||
        var->is_parameter || var->is_pack || var->is_handler_param ||
        (var->type != nullptr && type_is_nonreal(var->type))) {
      return llvm_gen_be_error_t::ok;
    }

    std::string sym_name;
    LLVM_GEN_BE_CHECK(llvm_get_mangled_variable_name(var, &sym_name));
    
    llvm::Type* llvm_ty = nullptr;
    llvm_gen_be_error_t err = get_llvm_type(var->type, &llvm_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::GlobalValue::LinkageTypes linkage = llvm::GlobalValue::ExternalLinkage;
    if (var->storage_class == sc_static || var->source_corresp.name_linkage == nlk_internal) {
      linkage = llvm::GlobalValue::InternalLinkage;
    }
    if (var->is_inline || var->comdat_group != nullptr ||
        llvm::StringRef(sym_name).starts_with("_ZZ")) {
      linkage = llvm::GlobalValue::LinkOnceODRLinkage;
    }
    if (var->explicit_instantiation) {
      linkage = llvm::GlobalValue::WeakODRLinkage;
    }
#if GNU_EXTENSIONS_ALLOWED
    if (var->is_weak || var->is_weakref) {
      linkage = llvm::GlobalValue::WeakAnyLinkage;
    }
#endif

    an_init_kind init_kind;
    an_initializer_ptr initializer_ptr;
    get_variable_initializer(var, il_header.primary_scope, &init_kind, &initializer_ptr);

    bool is_fn_local_static =
        (linkage == llvm::GlobalValue::InternalLinkage) &&
        ((il_header.primary_scope != nullptr &&
          (il_header.primary_scope->kind == sck_function ||
           il_header.primary_scope->kind == sck_block)) ||
         (var->source_corresp.parent_scope != nullptr &&
          (var->source_corresp.parent_scope->kind == sck_function ||
           var->source_corresp.parent_scope->kind == sck_block)));

    auto existing_var_it = be_state->local_vars.find(var);
    llvm::GlobalVariable* gvar = nullptr;
    bool had_existing_gvar = false;
    if (existing_var_it != be_state->local_vars.end()) {
      gvar = llvm::dyn_cast<llvm::GlobalVariable>(existing_var_it->second);
      had_existing_gvar = (gvar != nullptr);
    }
    if (!gvar) {
      if (is_fn_local_static && be_state->module->getNamedGlobal(sym_name) != nullptr) {
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(
            be_state->module.get(), llvm_ty, false, linkage, nullptr, sym_name, &gvar));
      } else {
        had_existing_gvar = (be_state->module->getNamedGlobal(sym_name) != nullptr);
        be_state->module->getOrInsertGlobal(sym_name, llvm_ty);
        gvar = be_state->module->getNamedGlobal(sym_name);
      }
    }
    if (gvar && gvar->getValueType() != llvm_ty) {
      llvm::GlobalVariable* replaced_gv = nullptr;
      LLVM_GEN_BE_CHECK(llvm_create_global_variable(
          be_state->module.get(), llvm_ty, false, linkage, nullptr, "", &replaced_gv));
      gvar->replaceAllUsesWith(replaced_gv);
      gvar->eraseFromParent();
      replaced_gv->setName(sym_name);
      gvar = replaced_gv;
    }
    if (gvar) {
      be_state->local_vars[var] = gvar;
    }

    llvm::Constant* llvm_init = nullptr;
    if (init_kind == initk_static) {
       llvm_gen_be_error_t eval_err = evaluate_constant(initializer_ptr->constant, llvm_ty, &llvm_init);
       if (eval_err != llvm_gen_be_error_t::ok) {
         if (!had_existing_gvar && gvar) {
           be_state->local_vars.erase(var);
           gvar->eraseFromParent();
         }
         return eval_err;
       }
    } else if (init_kind == initk_dynamic) {
       a_dynamic_init_ptr dip = initializer_ptr->dynamic;
       if (dip) {
         if (dip->kind == dik_constant && !dip->follows_an_exec_statement) {
           llvm_init = llvm::Constant::getNullValue(llvm_ty);
         }
         if (dip->kind != dik_constant || dip->follows_an_exec_statement || dip->destructor != nullptr) {
           GlobalDynamicInitEntry entry = {};
           entry.var = var;
           entry.dip = dip;
           be_state->global_dynamic_inits.push_back(entry);
         }
       }
    }

    if (!llvm_init && var->storage_class != sc_extern) {
       // Default to zero-initialized for static/global without explicit init
       llvm_init = llvm::Constant::getNullValue(llvm_ty);
    }
    
    if (!llvm_init) {
       linkage = llvm::GlobalValue::ExternalLinkage;
    }

    if (gvar) {
      if (llvm_init && gvar->getValueType() != llvm_init->getType()) {
        llvm::GlobalVariable* retyped_gv = nullptr;
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(
            be_state->module.get(), llvm_init->getType(), false, linkage, nullptr, "", &retyped_gv));
        gvar->replaceAllUsesWith(retyped_gv);
        gvar->eraseFromParent();
        retyped_gv->setName(sym_name);
        gvar = retyped_gv;
        be_state->local_vars[var] = gvar;
      }
      gvar->setLinkage(linkage);
      LLVM_GEN_BE_CHECK(apply_llvm_attributes(gvar, var->source_corresp.attributes));
#if GNU_EXTENSIONS_ALLOWED && GNU_VISIBILITY_ATTRIBUTE_ALLOWED
      LLVM_GEN_BE_CHECK(apply_elf_visibility(
          gvar, static_cast<an_ELF_visibility_kind>(var->ELF_visibility)));
#endif
      if (var->is_thread_local) {
        llvm::GlobalValue::ThreadLocalMode tls_mode = llvm::GlobalValue::GeneralDynamicTLSModel;
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
        for (an_attribute_ptr attr = var->source_corresp.attributes; attr != nullptr; attr = attr->next) {
            if (attr->kind == ak_tls_model) {
                std::string model_str;
                LLVM_GEN_BE_CHECK(extract_attr_string(attr, &model_str));
                if (model_str == "global-dynamic") {
                    tls_mode = llvm::GlobalValue::GeneralDynamicTLSModel;
                } else if (model_str == "local-dynamic") {
                    tls_mode = llvm::GlobalValue::LocalDynamicTLSModel;
                } else if (model_str == "initial-exec") {
                    tls_mode = llvm::GlobalValue::InitialExecTLSModel;
                } else if (model_str == "local-exec") {
                    tls_mode = llvm::GlobalValue::LocalExecTLSModel;
                }
            }
        }
#endif
        gvar->setThreadLocalMode(tls_mode);
      }
      bool is_macho = llvm::Triple(be_state->module->getTargetTriple()).isOSBinFormatMachO();
#if DO_IL_LOWERING
      if (var->comdat_group && !is_macho) {
        llvm::Comdat *comdat = be_state->module->getOrInsertComdat(var->comdat_group);
        gvar->setComdat(comdat);
      }
#endif
      if (!is_macho && !gvar->hasComdat() &&
          (linkage == llvm::GlobalValue::LinkOnceODRLinkage ||
           linkage == llvm::GlobalValue::WeakODRLinkage)) {
        llvm::Comdat *comdat = be_state->module->getOrInsertComdat(sym_name);
        comdat->setSelectionKind(llvm::Comdat::Any);
        gvar->setComdat(comdat);
      } else if (is_macho && be_state->explicit_default_vis.count(gvar) == 0) {
        gvar->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
      }
      if (llvm_init) {
        gvar->setInitializer(llvm_init);
      }
      if (be_state->dbg_state) {
        llvm_gen_be_error_t dbg_err = emit_dbg_global_variable(be_state->dbg_state, var, gvar);
        if (dbg_err != llvm_gen_be_error_t::ok) return dbg_err;
      }
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Extracts the associated scope of a class, struct, or union type if valid and non-placeholder.
 * @details Returns `nullptr` for non-aggregate types, incomplete classes, or placeholder scopes.
 * @param[in] tp The EDG type pointer to inspect.
 * @param[out] out_scope Receives the valid associated `a_scope_ptr`, or `nullptr`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t get_valid_class_assoc_scope(
    a_type_ptr tp,
    a_scope_ptr* out_scope) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_scope = nullptr;
  if (!ignore_type_in_back_end(tp) &&
      (tp->kind == tk_class || tp->kind == tk_struct || tp->kind == tk_union) &&
      !tp->variant.class_struct_union.is_prototype_instantiation &&
      !(il_header.function_def_table != nullptr &&
        tp->variant.class_struct_union.is_template_class &&
        !tp->variant.class_struct_union.is_specialized) &&
      tp->variant.class_struct_union.extra_info &&
      tp->variant.class_struct_union.extra_info->assoc_scope &&
      !tp->variant.class_struct_union.extra_info->assoc_scope->is_placeholder_scope) {
    *out_scope = tp->variant.class_struct_union.extra_info->assoc_scope;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Recursively lowers global and static variables within a scope, its namespaces, and its classes.
 * @details Traverses `scope->variables`, `scope->namespaces`, `scope->types` (for class/struct
 *          associated scopes), and `scope->scopes` to lower all static storage duration variables.
 * @param[in] scope The EDG scope to traverse.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t lower_variables_in_scope(a_scope_ptr scope) noexcept {
  if (!be_state->visited_scopes.insert(scope).second) {
    return llvm_gen_be_error_t::ok;
  }
  bool is_primary = (scope == il_header.primary_scope);
  bool is_local_scope = (scope->kind == sck_function || scope->kind == sck_block);
  for (a_variable_ptr var = scope->variables; var != nullptr; var = var->next) {
    if (is_local_scope && var->storage_class != sc_static) {
      continue;
    }
    if (!is_primary && il_header.function_def_table != nullptr &&
        var->storage_class == sc_extern && var->init_kind == initk_none) {
      continue;
    }
    llvm_gen_be_error_t err = llvm_lower_global_variable(var);
    if (err != llvm_gen_be_error_t::ok) return err;
  }
  for (a_namespace_ptr ns = scope->namespaces; ns != nullptr; ns = ns->next) {
    if (!ns->is_namespace_alias && ns->variant.assoc_scope) {
      llvm_gen_be_error_t err = lower_variables_in_scope(ns->variant.assoc_scope);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  }
  for (a_type_ptr tp = scope->types; tp != nullptr; tp = tp->next) {
    if (ignore_type_in_back_end(tp)) continue;
#if DO_IL_LOWERING
    const bool edg_lowered_vtables = (il_lowering_needed() != FALSE);
#else /* !DO_IL_LOWERING */
    const bool edg_lowered_vtables = false;
#endif /* DO_IL_LOWERING */
    if (!edg_lowered_vtables &&
        (tp->kind == tk_class || tp->kind == tk_struct) &&
        !tp->variant.class_struct_union.is_prototype_instantiation) {
      if (tp->variant.class_struct_union.any_virtual_functions_including_in_base_classes) {
        llvm::Constant* vtbl_const = nullptr;
        llvm_gen_be_error_t v_err = llvm_emit_vtable(tp, &vtbl_const);
        if (v_err != llvm_gen_be_error_t::ok) return v_err;
      }
      if (tp->variant.class_struct_union.any_virtual_base_classes) {
        llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
        if (triple.isWindowsMSVCEnvironment()) {
          if (!tp->variant.class_struct_union.any_virtual_functions_including_in_base_classes) {
            llvm::Constant* vb_const = nullptr;
            llvm_gen_be_error_t vb_err = llvm_emit_msvc_vbtable(tp, &vb_const);
            if (vb_err != llvm_gen_be_error_t::ok) return vb_err;
          }
        } else {
          llvm::Constant* vtt_const = nullptr;
          llvm_gen_be_error_t vtt_err = llvm_emit_vtt(tp, &vtt_const);
          if (vtt_err != llvm_gen_be_error_t::ok) return vtt_err;
        }
      }
    }
    a_scope_ptr cscope = nullptr;
    LLVM_GEN_BE_CHECK(get_valid_class_assoc_scope(tp, &cscope));
    if (cscope) {
      llvm_gen_be_error_t err = lower_variables_in_scope(cscope);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  }
  for (a_scope_ptr sub = scope->scopes; sub != nullptr; sub = sub->next) {
    if (sub->kind != sck_func_prototype && !sub->is_expansion_prototype) {
      llvm_gen_be_error_t err = lower_variables_in_scope(sub);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers all EDG global variables in the translation unit.
 * @details Traverses the primary file scope and all nested namespace and class scopes.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_lower_global_variables(void) noexcept {
  be_state->visited_scopes.clear();
  if (!il_header.primary_scope) return llvm_gen_be_error_t::ok;
  return lower_variables_in_scope(il_header.primary_scope);
}

/**
 * @brief Lowers a single EDG routine prototype into an LLVM Function.
 * @details Implements or declares `llvm_lower_function_prototype` for the EDG LLVM backend.
 * @param[in] routine The EDG routine.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_lower_function_prototype(a_routine_ptr routine) noexcept {
    if (!routine || !routine->source_corresp.name ||
        ignore_routine_in_back_end(routine) ||
        routine->is_deleted ||
        (routine->pure_virtual && routine->function_def_number == NULL_function_def_number) ||
        (routine->primary_ctor_or_dtor && routine->primary_ctor_or_dtor->is_deleted) ||
        (routine->type != nullptr && type_is_nonreal(routine->type))) {
      return llvm_gen_be_error_t::ok;
    }
    
    llvm::FunctionType* func_ty = nullptr;
    llvm::AttributeList attrs;
    llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
    llvm_gen_be_error_t err = llvm_build_target_abi_function_type(
        routine->type, false, 0, {}, &func_ty, &attrs);
    if (err != llvm_gen_be_error_t::ok) return err;
    if (!func_ty) return llvm_gen_be_error_t::ok;

    bool is_macho_fn = triple.isOSBinFormatMachO();
    llvm::GlobalValue::LinkageTypes linkage = llvm::GlobalValue::ExternalLinkage;
    llvm::Function* existing_fn = be_state->module->getFunction(routine->source_corresp.name);
    llvm::Function* func = nullptr;
    if (existing_fn && existing_fn->getFunctionType() == func_ty) {
      func = existing_fn;
    } else {
      func = llvm::Function::Create(func_ty, linkage, "", be_state->module.get());
      if (existing_fn) {
        existing_fn->replaceAllUsesWith(func);
        existing_fn->eraseFromParent();
      }
      func->setName(routine->source_corresp.name);
    }
    func->setAttributes(attrs);
    LLVM_GEN_BE_CHECK(apply_llvm_attributes(func, routine->source_corresp.attributes));
#if GNU_EXTENSIONS_ALLOWED && GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    LLVM_GEN_BE_CHECK(apply_elf_visibility(
        func, static_cast<an_ELF_visibility_kind>(routine->ELF_visibility)));
#endif

    if (routine->function_def_number != 0) {
       if (routine->storage_class == sc_static || routine->source_corresp.name_linkage == nlk_internal) {
         linkage = llvm::GlobalValue::InternalLinkage;
       } else if (routine->explicit_instantiation) {
         linkage = llvm::GlobalValue::WeakODRLinkage;
       } else if (routine->is_inline ||
                  (routine->is_template_function && !routine->is_specialized) ||
                  routine->use_comdat || routine->compiler_generated ||
                  llvm::StringRef(routine->source_corresp.name).starts_with("_ZZ")) {
         linkage = llvm::GlobalValue::LinkOnceODRLinkage;
       }
#if GNU_EXTENSIONS_ALLOWED
       if (routine->has_internal_linkage_attribute) {
         linkage = llvm::GlobalValue::InternalLinkage;
       }
#endif
       func->setLinkage(linkage);
#if DO_IL_LOWERING && IA64_ABI
       if (routine->use_comdat && !is_macho_fn) {
           llvm::Comdat *comdat = be_state->module->getOrInsertComdat(routine->source_corresp.name);
           comdat->setSelectionKind(llvm::Comdat::Any);
           func->setComdat(comdat);
       }
#endif
    }

    if (!is_macho_fn && !func->hasComdat() &&
        (linkage == llvm::GlobalValue::LinkOnceODRLinkage ||
         linkage == llvm::GlobalValue::WeakODRLinkage)) {
        llvm::Comdat *comdat = be_state->module->getOrInsertComdat(routine->source_corresp.name);
        comdat->setSelectionKind(llvm::Comdat::Any);
        func->setComdat(comdat);
    } else if (is_macho_fn && be_state->explicit_default_vis.count(func) == 0) {
        func->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    }
        
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
    if (routine->always_inline) {
       func->addFnAttr(llvm::Attribute::AlwaysInline);
    }
    if (routine->never_inline) {
       func->addFnAttr(llvm::Attribute::NoInline);
    }
    if (routine->is_pure) {
       func->setOnlyReadsMemory();
    }
#endif
    if (routine->has_nodiscard_attribute) {
       func->addFnAttr("warn-unused-result");
    }
    extern a_boolean sanitize_address;
    extern a_boolean sanitize_thread;
    extern a_boolean sanitize_memory;
    extern a_boolean sanitize_undefined;
    extern a_boolean sanitize_hwaddress;
    extern a_boolean use_safestack;
    extern a_boolean use_shadowcallstack;
    extern int stack_protector_mode;
    if (sanitize_address) func->addFnAttr(llvm::Attribute::SanitizeAddress);
    if (sanitize_thread) func->addFnAttr(llvm::Attribute::SanitizeThread);
    if (sanitize_memory) func->addFnAttr(llvm::Attribute::SanitizeMemory);
    if (sanitize_undefined) func->addFnAttr("ubsan");
    if (sanitize_hwaddress) func->addFnAttr(llvm::Attribute::SanitizeHWAddress);
    if (use_safestack) func->addFnAttr(llvm::Attribute::SafeStack);
    if (use_shadowcallstack) func->addFnAttr(llvm::Attribute::ShadowCallStack);
    if (stack_protector_mode == 1) func->addFnAttr(llvm::Attribute::StackProtect);
    else if (stack_protector_mode == 2) func->addFnAttr(llvm::Attribute::StackProtectStrong);
    else if (stack_protector_mode == 3) func->addFnAttr(llvm::Attribute::StackProtectReq);

    // Basic calling convention, noreturn, noexcept, and restrict parameter mapping
    if (routine->type->kind == tk_routine) {
        a_routine_type_supplement_ptr supp = routine->type->variant.routine.extra_info;
        if (supp) {
#if MICROSOFT_EXTENSIONS_ALLOWED
            if (supp->calling_convention == cc_stdcall) func->setCallingConv(llvm::CallingConv::X86_StdCall);
            else if (supp->calling_convention == cc_fastcall) func->setCallingConv(llvm::CallingConv::X86_FastCall);
            else if (supp->calling_convention == cc_thiscall) func->setCallingConv(llvm::CallingConv::X86_ThisCall);
            else if (supp->calling_convention == cc_vectorcall) func->setCallingConv(llvm::CallingConv::X86_VectorCall);
#endif
            if (supp->does_not_return) {
                func->addFnAttr(llvm::Attribute::NoReturn);
            }
            if (supp->exception_specification && supp->exception_specification->is_noexcept) {
                func->addFnAttr(llvm::Attribute::NoUnwind);
            }
            unsigned param_idx = 0;
            for (a_param_type_ptr param = supp->param_type_list;
                 param != nullptr; param = param->next, ++param_idx) {
              if (param->type && (f_get_type_qualifiers(param->type, TRUE) & TQ_RESTRICT) != 0) {
                func->addParamAttr(param_idx, llvm::Attribute::NoAlias);
              }
            }
        }
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Determines whether an unreferenced `static inline` routine in a full TU should be skipped.
 * @details When `il_header.function_def_table != nullptr` (front-end compilation with `MAINTAIN_NEEDED_FLAGS`),
 *          `static inline` wrappers (e.g., thousands of unused intrinsics in `<arm_neon.h>` and `<immintrin.h>`)
 *          whose `definition_needed` flag is false are unreferenced and must not be emitted.
 * @param[in] routine Pointer to the EDG routine node to inspect.
 * @param[out] out_skip Receives `true` if @p routine is an unreferenced `static inline` routine, else `false`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t is_unused_static_inline_routine(
    a_routine_ptr routine,
    bool* out_skip) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
#if MAINTAIN_NEEDED_FLAGS
  bool needed = (routine->definition_needed != FALSE);
#else
  bool needed = (routine->source_corresp.referenced != FALSE);
#endif
  *out_skip = (il_header.function_def_table != nullptr &&
               routine->is_inline &&
               routine->storage_class == sc_static &&
               !needed);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Gets the lexical scope for a routine definition.
 * @details Resolves `scope_for_routine`, `il_header.function_def_table`, or `il_header.region_scope_entry`.
 * @param[in] rout The EDG routine.
 * @param[out] out_scope Pointer to store the resulting scope.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
static llvm_gen_be_error_t get_scope_for_routine_definition(a_routine_ptr rout, a_scope_ptr* out_scope) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  a_memory_region_number region_number = mem_region_for_routine(rout);
  a_scope_ptr res = scope_for_routine(rout);
  if (!res && il_header.function_def_table) {
     a_function_def_descr def_descr = il_header.function_def_table[rout->function_def_number];
     res = def_descr.scope;
  }
  if (!res && region_number != NULL_region_number && il_header.region_scope_entry) {
     res = il_header.region_scope_entry[region_number];
  }
  *out_scope = res;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Recursively lowers routine declarations within a scope, its namespaces, and its classes.
 * @details Traverses `scope->routines`, `scope->namespaces`, `scope->types` (for class/struct
 *          member routines), and `scope->scopes` to emit LLVM function prototypes.
 * @param[in] scope The EDG scope to traverse.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t lower_routine_decls_in_scope(a_scope_ptr scope) noexcept {
  if (!be_state->visited_scopes.insert(scope).second) {
    return llvm_gen_be_error_t::ok;
  }
#if DO_IL_LOWERING
  const bool prune_unref = (il_lowering_needed() != FALSE);
#else /* !DO_IL_LOWERING */
  const bool prune_unref = false;
#endif /* DO_IL_LOWERING */
  bool is_primary = (scope == il_header.primary_scope);
  for (a_routine_ptr routine = scope->routines; routine != nullptr; routine = routine->next) {
    if (prune_unref && !is_primary && il_header.function_def_table != nullptr &&
        routine->function_def_number == NULL_function_def_number &&
        !routine->source_corresp.referenced) {
      continue;
    }
    bool skip_rout = false;
    LLVM_GEN_BE_CHECK(is_unused_static_inline_routine(routine, &skip_rout));
    if (skip_rout) continue;
#if MAINTAIN_NEEDED_FLAGS || BACK_END_IS_GCC_GEN_BE
    if (!is_primary && il_header.function_def_table != nullptr &&
        !routine->definition_needed && !routine->source_corresp.referenced &&
        !routine->keep_definition_in_il) {
      continue;
    }
#endif /* MAINTAIN_NEEDED_FLAGS || BACK_END_IS_GCC_GEN_BE */
    llvm_gen_be_error_t err = llvm_lower_function_prototype(routine);
    if (err != llvm_gen_be_error_t::ok) return err;
    if (!ignore_routine_in_back_end(routine) &&
        routine->function_def_number != NULL_function_def_number) {
      a_scope_ptr fscope = nullptr;
      LLVM_GEN_BE_CHECK(get_scope_for_routine_definition(routine, &fscope));
      if (fscope) {
        LLVM_GEN_BE_CHECK(lower_routine_decls_in_scope(fscope));
      }
    }
  }
  for (a_namespace_ptr ns = scope->namespaces; ns != nullptr; ns = ns->next) {
    if (!ns->is_namespace_alias && ns->variant.assoc_scope) {
      llvm_gen_be_error_t err = lower_routine_decls_in_scope(ns->variant.assoc_scope);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  }
  for (a_type_ptr tp = scope->types; tp != nullptr; tp = tp->next) {
    a_scope_ptr cscope = nullptr;
    LLVM_GEN_BE_CHECK(get_valid_class_assoc_scope(tp, &cscope));
    if (cscope) {
      llvm_gen_be_error_t err = lower_routine_decls_in_scope(cscope);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  }
  for (a_scope_ptr sub = scope->scopes; sub != nullptr; sub = sub->next) {
    if (sub->kind != sck_func_prototype && !sub->is_expansion_prototype) {
      llvm_gen_be_error_t err = lower_routine_decls_in_scope(sub);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers all EDG routine declarations in the translation unit.
 * @details Traverses the primary file scope and all nested namespace and class scopes.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_lower_function_declarations(void) noexcept {
  be_state->visited_scopes.clear();
  if (!il_header.primary_scope) return llvm_gen_be_error_t::ok;
  LLVM_GEN_BE_CHECK(lower_routine_decls_in_scope(il_header.primary_scope));
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  for (a_scope_orphaned_list_header_ptr solhp = il_header.scope_orphaned_list_headers;
       solhp != nullptr; solhp = solhp->next) {
    if (solhp->in_expansion_prototype ||
        (solhp->assoc_routine && ignore_routine_in_back_end(solhp->assoc_routine))) continue;
    for (a_type_ptr tp = solhp->orphaned_types; tp != nullptr; tp = tp->next) {
      a_scope_ptr cscope = nullptr;
      LLVM_GEN_BE_CHECK(get_valid_class_assoc_scope(tp, &cscope));
      if (cscope) {
        LLVM_GEN_BE_CHECK(lower_routine_decls_in_scope(cscope));
      }
    }
  }
#endif
  return llvm_gen_be_error_t::ok;
}

#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
/**
 * @brief Lowers a virtual/covariant return adjustment thunk routine body into LLVM IR.
 * @details Adjusts the `this` pointer using `delta` and `vcall_index`, invokes the target
 *          overriding method, and adjusts the returned pointer using `return_delta` and `vbase_index`.
 * @param[in] routine The EDG wrapper/thunk routine descriptor.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t llvm_lower_thunk_body(a_routine_ptr routine) noexcept {
    if (!routine->source_corresp.name) return llvm_gen_be_error_t::ok;
    
    llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
    if (!func || !func->empty()) return llvm_gen_be_error_t::ok;
    
    a_routine_ptr overriding = routine->overriding_function_for_wrapper;
    llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*be_state->context, "entry", func);
    be_state->builder->SetInsertPoint(entry_bb);
    
    llvm::Function* target_func = be_state->module->getFunction(overriding->source_corresp.name ? overriding->source_corresp.name : "");
    if (!target_func) return llvm_gen_be_error_t::code_gen_failure; // Target must exist
    
    // Adjust 'this' pointer
    auto arg_it = func->arg_begin();
    if (arg_it == func->arg_end()) return llvm_gen_be_error_t::ok; // Expect at least 'this'
    
    llvm::Value* this_val = &*arg_it;
    llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
    bool is_msvc = triple.isWindowsMSVCEnvironment();
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
    
    // Virtual offset (vcall_index / MSVC vtordisp)
#if IA64_ABI
    if (routine->vcall_index != 0) {
        if (is_msvc) {
            // MSVC vtordisp: load 32-bit displacement from (this - 4) + delta and subtract
            llvm::Value* vtordisp_ptr = be_state->builder->CreateInBoundsGEP(
                i8_ty, this_val, llvm::ConstantInt::getSigned(i64_ty, -4), "vtordisp.ptr");
            llvm::Value* vtordisp_val = be_state->builder->CreateLoad(i32_ty, vtordisp_ptr, "vtordisp.val");
            llvm::Value* vtordisp_neg = be_state->builder->CreateNeg(
                be_state->builder->CreateSExt(vtordisp_val, i64_ty), "vtordisp.neg");
            this_val = be_state->builder->CreateInBoundsGEP(i8_ty, this_val, vtordisp_neg, "this.adj.vtordisp");
        } else {
            llvm::Value* vptr = be_state->builder->CreateLoad(ptr_ty, this_val, "vptr");
            llvm::Value* vcall_offset_idx = llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(routine->vcall_index));
            llvm::Value* vcall_offset_ptr = be_state->builder->CreateInBoundsGEP(ptr_ty, vptr, vcall_offset_idx);
            llvm::Value* vcall_offset_val = be_state->builder->CreateLoad(i64_ty, vcall_offset_ptr, "vcall.offset");
            this_val = be_state->builder->CreateInBoundsGEP(i8_ty, this_val, vcall_offset_val, "this.adj.v");
        }
    }
#endif

    // Non-virtual offset (delta)
#if IA64_ABI
    if (routine->delta != 0) {
        llvm::Value* offset = llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(routine->delta));
        this_val = be_state->builder->CreateInBoundsGEP(i8_ty, this_val, offset, "this.adj.nv");
    }
#endif
    
    // Call target function
    std::vector<llvm::Value*> call_args;
    call_args.push_back(this_val); // Adjusted this
    
    ++arg_it; // Skip this
    for (; arg_it != func->arg_end(); ++arg_it) {
        call_args.push_back(&*arg_it);
    }
    
    llvm::CallInst* call = be_state->builder->CreateCall(target_func, call_args);
    call->setTailCall(true);
    
    // Covariant return adjustment
#if IA64_ABI
    if (routine->return_delta != 0 || routine->vbase_index != 0) {
        llvm::Value* ret_val = call;
        
        llvm::Value* null_check = be_state->builder->CreateIsNull(ret_val);
        llvm::BasicBlock* adj_bb = llvm::BasicBlock::Create(*be_state->context, "adj", func);
        llvm::BasicBlock* cont_bb = llvm::BasicBlock::Create(*be_state->context, "cont", func);
        
        be_state->builder->CreateCondBr(null_check, cont_bb, adj_bb);
        
        be_state->builder->SetInsertPoint(adj_bb);
        llvm::Value* adj_ret = ret_val;
        
        // Virtual base adjustment for return value
        if (routine->vbase_index != 0) {
            if (is_msvc) {
                llvm::Value* vbptr = be_state->builder->CreateLoad(ptr_ty, adj_ret, "ret.vbptr");
                llvm::Value* vb_idx = llvm::ConstantInt::get(i32_ty, static_cast<uint64_t>(routine->vbase_index));
                llvm::Value* vb_slot = be_state->builder->CreateInBoundsGEP(i32_ty, vbptr, vb_idx, "ret.vb.slot");
                llvm::Value* vb_off = be_state->builder->CreateLoad(i32_ty, vb_slot, "ret.vb.off");
                adj_ret = be_state->builder->CreateInBoundsGEP(i8_ty, adj_ret, vb_off, "ret.adj.vbase");
            } else {
                llvm::Value* vptr = be_state->builder->CreateLoad(ptr_ty, adj_ret, "ret.vptr");
                llvm::Value* vbase_offset_idx = llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(routine->vbase_index));
                llvm::Value* vbase_offset_ptr = be_state->builder->CreateInBoundsGEP(ptr_ty, vptr, vbase_offset_idx);
                llvm::Value* vbase_offset_val = be_state->builder->CreateLoad(i64_ty, vbase_offset_ptr, "vbase.offset");
                adj_ret = be_state->builder->CreateInBoundsGEP(i8_ty, adj_ret, vbase_offset_val, "ret.adj.vbase");
            }
        }
        
        // Non-virtual base adjustment
        if (routine->return_delta != 0) {
            llvm::Value* ret_offset = llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(routine->return_delta));
            adj_ret = be_state->builder->CreateInBoundsGEP(i8_ty, adj_ret, ret_offset, "ret.adj.nv");
        }
        
        be_state->builder->CreateBr(cont_bb);
#endif

        be_state->builder->SetInsertPoint(cont_bb);
        llvm::PHINode* phi = be_state->builder->CreatePHI(adj_ret->getType(), 2);
        phi->addIncoming(llvm::Constant::getNullValue(adj_ret->getType()), entry_bb);
        phi->addIncoming(adj_ret, adj_bb);
        
        be_state->builder->CreateRet(phi);
    } else {
        if (func->getReturnType()->isVoidTy()) {
            be_state->builder->CreateRetVoid();
        } else {
            be_state->builder->CreateRet(call);
        }
    }
    
    return llvm_gen_be_error_t::ok;
}
#endif

/**
 * @brief Lowers constructor member, base class, and delegating initializers.
 * @details Iterates over `func_scope->variant.routine.constructor_inits` and emits field/base GEPs
 *          followed by `llvm_emit_dynamic_init`. For base subobject (`C2`) constructor calls on classes
 *          with virtual bases, passes the sub-VTT element pointer (`getelementptr` into `_ZTT<D>`) when
 *          the base constructor accepts an implicit VTT parameter.
 * @param[in] routine The EDG constructor routine being lowered.
 * @param[in] func_scope The EDG scope of the constructor definition.
 * @param[in] func The LLVM Function being lowered.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t llvm_lower_constructor_inits(
    a_routine_ptr routine,
    a_scope_ptr func_scope,
    llvm::Function* func) noexcept {
    (void)func;
    a_variable_ptr this_var = func_scope->variant.routine.parameters;
    if (!this_var) return llvm_gen_be_error_t::ok; // No 'this' pointer, maybe not a constructor
    llvm::Value* this_ptr = be_state->local_vars[this_var];
    if (!this_ptr) return llvm_gen_be_error_t::internal_inconsistency;

    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
    llvm::Value* obj_ptr = be_state->builder->CreateLoad(ptr_ty, this_ptr);
    size_t init_cleanup_depth = be_state->cleanup_stack.cleanups.size();

    a_type_ptr derived_class_tp = nullptr;
    if (this_var->type && skip_typerefs(this_var->type)->kind == tk_pointer &&
        skip_typerefs(this_var->type)->variant.pointer.type) {
      derived_class_tp = skip_typerefs(skip_typerefs(this_var->type)->variant.pointer.type);
    }
    llvm::Constant* derived_vtt = nullptr;
    llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
    if (!triple.isWindowsMSVCEnvironment() && derived_class_tp &&
        derived_class_tp->variant.class_struct_union.any_virtual_base_classes &&
        routine->ctor_dtor_kind != cdk_subobject) {
      LLVM_GEN_BE_CHECK(llvm_emit_vtt(derived_class_tp, &derived_vtt));
    }
    uint64_t sub_vtt_idx = 1;

    for (a_constructor_init_ptr init = func_scope->variant.routine.constructor_inits; init != nullptr; init = init->next) {
        a_dynamic_init_ptr dip = init->initializer;
        if (!dip) continue;

        llvm::Value* target_ptr = nullptr;
        llvm::Type* target_ty = nullptr;

        if (init->kind == cik_field) {
            a_field_ptr field = init->variant.field;
            if (!field) continue;
            llvm_gen_be_error_t err = get_llvm_type(field->type, &target_ty);
            if (err != llvm_gen_be_error_t::ok) return err;

            llvm::Value* offset_val = llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(field->offset));
            target_ptr = be_state->builder->CreateInBoundsGEP(llvm::Type::getInt8Ty(*be_state->context), obj_ptr, offset_val);

        } else if (init->kind == cik_direct_base_class || init->kind == cik_virtual_base_class) {
            a_base_class_ptr base = init->variant.base_class;
            if (!base) continue;
            llvm_gen_be_error_t err = get_llvm_type(base->type, &target_ty);
            if (err != llvm_gen_be_error_t::ok) return err;

            llvm::Value* offset_val = llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(base->offset));
            target_ptr = be_state->builder->CreateInBoundsGEP(llvm::Type::getInt8Ty(*be_state->context), obj_ptr, offset_val);

            if (derived_vtt && dip->kind == dik_constructor) {
                a_routine_ptr c2_r = dip->variant.constructor.ptr;
                llvm::Function* base_ctor_fn = (c2_r && c2_r->source_corresp.name)
                    ? be_state->module->getFunction(c2_r->source_corresp.name)
                    : nullptr;
                if (!base_ctor_fn && c2_r && c2_r->source_corresp.name && c2_r->type) {
                    llvm::FunctionType* b_fty = nullptr;
                    llvm::AttributeList b_attrs;
                    if (llvm_build_target_abi_function_type(c2_r->type, false, 0, {}, &b_fty, &b_attrs) == llvm_gen_be_error_t::ok && b_fty) {
                        base_ctor_fn = llvm::Function::Create(
                            b_fty, llvm::GlobalValue::ExternalLinkage, c2_r->source_corresp.name, be_state->module.get());
                        if (!b_attrs.isEmpty()) base_ctor_fn->setAttributes(b_attrs);
                    }
                }
                if (base_ctor_fn && base_ctor_fn->arg_size() >= 2) {
                    llvm::Value* sub_vtt_ptr = be_state->builder->CreateInBoundsGEP(
                        ptr_ty, derived_vtt, llvm::ConstantInt::get(i64_ty, sub_vtt_idx), "sub.vtt.ptr");
                    ++sub_vtt_idx;
                    llvm::BasicBlock* lpad_bb = nullptr;
                    if (!base_ctor_fn->doesNotThrow()) {
                        LLVM_GEN_BE_CHECK(llvm_get_or_create_current_unwind_dest(&lpad_bb));
                    }
                    if (lpad_bb) {
                        llvm::BasicBlock* cont_bb = llvm::BasicBlock::Create(*be_state->context, "base.c2.cont", func);
                        be_state->builder->CreateInvoke(base_ctor_fn, cont_bb, lpad_bb, {target_ptr, sub_vtt_ptr});
                        be_state->builder->SetInsertPoint(cont_bb);
                    } else {
                        be_state->builder->CreateCall(base_ctor_fn, {target_ptr, sub_vtt_ptr});
                    }
                    if (dip->destructor) {
                        LLVM_GEN_BE_CHECK(llvm_emit_eh_cleanup(dip->destructor, target_ptr));
                    }
                    continue;
                }
            }
        } else if (init->kind == cik_delegation) {
            target_ptr = obj_ptr;
        }

        if (target_ptr) {
            llvm_gen_be_error_t err = llvm_emit_dynamic_init(dip, target_ptr, target_ty);
            if (err != llvm_gen_be_error_t::ok) return err;
            if (dip->destructor) {
                LLVM_GEN_BE_CHECK(llvm_emit_eh_cleanup(dip->destructor, target_ptr));
            }
        }
    }
    while (be_state->cleanup_stack.cleanups.size() > init_cleanup_depth) {
        be_state->cleanup_stack.cleanups.pop_back();
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Initializes or resets `vptr` and `vbptr` slots in constructors and destructors.
 * @details Stores the Itanium vtable address point (or VTT subobject vtable pointer in C2/D2)
 *          or MSVC `vftable`/`vbtable` pointers into the `this` object and secondary base subobjects.
 * @param[in] routine The EDG constructor or destructor routine being lowered.
 * @param[in] func_scope The EDG scope of the routine definition.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t llvm_emit_ctor_dtor_vptr_init(
    a_routine_ptr routine,
    a_scope_ptr func_scope) noexcept {
  bool is_ctor = (routine->special_kind == sfk_constructor ||
                  func_scope->variant.routine.constructor_inits != nullptr);
  bool is_dtor = (routine->special_kind == sfk_destructor);
  bool is_subobject_ctor_dtor = false;
#if DO_IL_LOWERING && IA64_ABI
  if (routine->ctor_dtor_kind == cdk_complete ||
      routine->ctor_dtor_kind == cdk_subobject) {
    is_ctor = true;
  }
  if (routine->ctor_dtor_kind == cdk_deleting) {
    is_dtor = true;
  }
  if (routine->ctor_dtor_kind == cdk_subobject) {
    is_subobject_ctor_dtor = true;
  }
#endif
  std::string rname = std::string(routine->source_corresp.name);
  if (rname.find("C2E") != std::string::npos || rname.find("D2E") != std::string::npos) {
    is_subobject_ctor_dtor = true;
  }
  if (!is_ctor && !is_dtor) {
    return llvm_gen_be_error_t::ok;
  }

  a_variable_ptr this_var = func_scope->variant.routine.parameters;
  if (!this_var || !this_var->type) {
    return llvm_gen_be_error_t::ok;
  }
  a_type_ptr this_ptr_ty = skip_typerefs(this_var->type);
  if (this_ptr_ty->kind != tk_pointer || !this_ptr_ty->variant.pointer.type) {
    return llvm_gen_be_error_t::ok;
  }
  a_type_ptr class_tp = skip_typerefs(this_ptr_ty->variant.pointer.type);
  if (class_tp->kind != tk_class && class_tp->kind != tk_struct) {
    return llvm_gen_be_error_t::ok;
  }
  if (!class_tp->variant.class_struct_union.any_virtual_functions_including_in_base_classes &&
      !class_tp->variant.class_struct_union.any_virtual_base_classes) {
    return llvm_gen_be_error_t::ok;
  }

  llvm::Value* this_alloca = be_state->local_vars[this_var];
  if (!this_alloca) {
    return llvm_gen_be_error_t::ok;
  }
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
  llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
  llvm::Value* obj_ptr = be_state->builder->CreateLoad(ptr_ty, this_alloca, "this.vptr.obj");

  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  if (triple.isWindowsMSVCEnvironment()) {
    if (class_tp->variant.class_struct_union.any_virtual_functions_including_in_base_classes) {
      llvm::Constant* vtbl_const = nullptr;
      LLVM_GEN_BE_CHECK(llvm_emit_vtable(class_tp, &vtbl_const));
      be_state->builder->CreateStore(vtbl_const, obj_ptr);
    }
    if (class_tp->variant.class_struct_union.any_virtual_base_classes && !is_dtor) {
      llvm::Constant* vb_const = nullptr;
      LLVM_GEN_BE_CHECK(llvm_emit_msvc_vbtable(class_tp, &vb_const));
      int64_t vbptr_off = class_tp->variant.class_struct_union.any_virtual_functions_including_in_base_classes
                              ? static_cast<int64_t>(targ_sizeof_pointer)
                              : 0;
      llvm::Value* vb_slot = (vbptr_off > 0)
          ? be_state->builder->CreateInBoundsGEP(i8_ty, obj_ptr, llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(vbptr_off)), "vbptr.slot")
          : obj_ptr;
      be_state->builder->CreateStore(vb_const, vb_slot);
    }
    return llvm_gen_be_error_t::ok;
  }

  // Itanium ABI: C2/D2 subobject constructor/destructor with virtual bases uses VTT
  if (is_subobject_ctor_dtor &&
      class_tp->variant.class_struct_union.any_virtual_base_classes) {
    llvm::Constant* vtt_const = nullptr;
    LLVM_GEN_BE_CHECK(llvm_emit_vtt(class_tp, &vtt_const));
    a_variable_ptr vtt_param = this_var->next;
    if (vtt_param && be_state->local_vars.count(vtt_param)) {
      llvm::Value* vtt_alloca = be_state->local_vars[vtt_param];
      llvm::Value* vtt_ptr = be_state->builder->CreateLoad(ptr_ty, vtt_alloca, "vtt.arg");
      llvm::Value* sub_vptr = be_state->builder->CreateLoad(ptr_ty, vtt_ptr, "vtt.vptr");
      be_state->builder->CreateStore(sub_vptr, obj_ptr);
      return llvm_gen_be_error_t::ok;
    }
  }

  if (class_tp->variant.class_struct_union.any_virtual_functions_including_in_base_classes) {
    llvm::Constant* vtbl_const = nullptr;
    LLVM_GEN_BE_CHECK(llvm_emit_vtable(class_tp, &vtbl_const));
    a_class_type_supplement_ptr supp = class_tp->variant.class_struct_union.extra_info;
    int64_t prefix_slots = 2; // offset-to-top + RTTI pointer
    if (supp) {
      for (a_base_class_ptr b = supp->direct_base_classes; b != nullptr; b = b->next_direct) {
        if (b->is_virtual) {
          prefix_slots += 2; // vbase_offset + vcall_offset
        }
      }
    }
    llvm::Value* addr_point = be_state->builder->CreateInBoundsGEP(
        i8_ty, vtbl_const, llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(prefix_slots * 8)), "vtable.addr_point");
    be_state->builder->CreateStore(addr_point, obj_ptr);

    if (supp) {
      int64_t primary_vfuncs = supp->highest_virtual_function_number;
      int64_t sec_slot_offset = prefix_slots + primary_vfuncs;
      for (a_base_class_ptr b = supp->base_classes; b != nullptr; b = b->next) {
        bool is_primary = (supp->virtual_function_info_base_class == b);
        if (!is_primary &&
            b->type->variant.class_struct_union.any_virtual_functions_including_in_base_classes) {
          int64_t sec_addr_slots = sec_slot_offset + 2;
          llvm::Value* sec_addr_point = be_state->builder->CreateInBoundsGEP(
              i8_ty, vtbl_const, llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(sec_addr_slots * 8)), "sec.vtable.addr_point");
          llvm::Value* sec_obj_ptr = be_state->builder->CreateInBoundsGEP(
              i8_ty, obj_ptr, llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(b->offset)), "sec.this.ptr");
          be_state->builder->CreateStore(sec_addr_point, sec_obj_ptr);
          a_class_type_supplement_ptr b_supp = b->type->variant.class_struct_union.extra_info;
          sec_slot_offset = sec_addr_slots + (b_supp ? b_supp->highest_virtual_function_number : 0);
        }
      }
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Finds the destructor routine declared in a class type's associated scope, if any.
 * @details Scans `supp->assoc_scope->routines` for a routine with `special_kind == sfk_destructor`.
 * @param[in] class_tp Pointer to the EDG class or struct type.
 * @param[out] out_dtor Receives the discovered destructor `a_routine_ptr`, or `nullptr` if none.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t llvm_find_class_destructor(
    a_type_ptr class_tp,
    a_routine_ptr* out_dtor) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_dtor = nullptr;
  a_type_ptr utp = skip_typerefs(class_tp);
  if (!utp->variant.class_struct_union.extra_info ||
      !utp->variant.class_struct_union.extra_info->assoc_scope) {
    return llvm_gen_be_error_t::ok;
  }
  for (a_routine_ptr r = utp->variant.class_struct_union.extra_info->assoc_scope->routines;
       r != nullptr; r = r->next) {
    if (r->special_kind == sfk_destructor) {
      *out_dtor = r;
      return llvm_gen_be_error_t::ok;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Finds a class-specific `operator delete` routine in a class type's associated scope, if any.
 * @details Scans `supp->assoc_scope->routines` for `sfk_operator` named `operator delete` or `_ZdlPv`.
 * @param[in] class_tp Pointer to the EDG class or struct type.
 * @param[out] out_del Receives the discovered `operator delete` routine, or `nullptr` if none.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t llvm_find_class_operator_delete(
    a_type_ptr class_tp,
    a_routine_ptr* out_del) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_del = nullptr;
  a_type_ptr utp = skip_typerefs(class_tp);
  if (utp->kind != tk_class ||
      !utp->variant.class_struct_union.extra_info ||
      !utp->variant.class_struct_union.extra_info->assoc_scope) {
    return llvm_gen_be_error_t::ok;
  }
  for (a_routine_ptr r = utp->variant.class_struct_union.extra_info->assoc_scope->routines;
       r != nullptr; r = r->next) {
    if (r->source_corresp.name && llvm::StringRef(r->source_corresp.name) == "operator delete") {
      *out_del = r;
      return llvm_gen_be_error_t::ok;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits reverse-order non-static data member and base-class destructor calls in `~D()`.
 * @details Collects destructible fields (`field_list`), direct non-virtual bases (`direct_base_classes`),
 *          and (when not a `D2` base subobject destructor) virtual bases (`base_classes`), and invokes
 *          their destructors in reverse declaration order. Registers each remaining cleanup on
 *          `be_state->cleanup_stack` during emission so exception-unwind paths destroy remaining subobjects.
 * @param[in] routine The EDG destructor routine being lowered.
 * @param[in] func_scope The EDG scope of the destructor definition.
 * @param[in] func The LLVM Function being lowered.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t llvm_lower_destructor_cleanups(
    a_routine_ptr routine,
    a_scope_ptr func_scope,
    llvm::Function* func) noexcept {
  if (routine->special_kind != sfk_destructor) {
    return llvm_gen_be_error_t::ok;
  }
  a_variable_ptr this_var = func_scope->variant.routine.parameters;
  if (!this_var || !this_var->type) {
    return llvm_gen_be_error_t::ok;
  }
  a_type_ptr this_ptr_ty = skip_typerefs(this_var->type);
  if (this_ptr_ty->kind != tk_pointer || !this_ptr_ty->variant.pointer.type) {
    return llvm_gen_be_error_t::ok;
  }
  a_type_ptr class_tp = skip_typerefs(this_ptr_ty->variant.pointer.type);
  if (class_tp->kind != tk_class) {
    return llvm_gen_be_error_t::ok;
  }

  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
  llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
  llvm::Value* this_alloca = be_state->local_vars[this_var];
  llvm::Value* obj_ptr = be_state->builder->CreateLoad(ptr_ty, this_alloca, "this.dtor.obj");

  bool is_subobject_dtor = (routine->ctor_dtor_kind == cdk_subobject ||
                            llvm::StringRef(routine->source_corresp.name).contains("D2E"));

  std::vector<std::pair<llvm::Value*, a_routine_ptr>> dtor_actions;

  for (a_field_ptr f = class_tp->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
    if (!f->type) continue;
    a_type_ptr elem_ty = skip_typerefs(f->type);
    uint64_t array_elems = 0;
    if (elem_ty->kind == tk_array && elem_ty->variant.array.element_type) {
      array_elems = static_cast<uint64_t>(elem_ty->variant.array.variant.number_of_elements);
      elem_ty = skip_typerefs(elem_ty->variant.array.element_type);
    }
    a_routine_ptr f_dtor = nullptr;
    LLVM_GEN_BE_CHECK(llvm_find_class_destructor(elem_ty, &f_dtor));
    if (!f_dtor) continue;
    llvm::Value* f_ptr = be_state->builder->CreateInBoundsGEP(
        i8_ty, obj_ptr, llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(f->offset)), "dtor.field.ptr");
    if (array_elems > 0) {
      for (uint64_t idx = 0; idx < array_elems; ++idx) {
        llvm::Value* elem_ptr = be_state->builder->CreateInBoundsGEP(
            i8_ty, f_ptr, llvm::ConstantInt::get(i64_ty, idx * static_cast<uint64_t>(elem_ty->size)), "dtor.arr.elem");
        dtor_actions.push_back({elem_ptr, f_dtor});
      }
    } else {
      dtor_actions.push_back({f_ptr, f_dtor});
    }
  }

  a_class_type_supplement_ptr supp = class_tp->variant.class_struct_union.extra_info;
  std::vector<std::pair<a_base_class_ptr, a_routine_ptr>> direct_nv_bases;
  std::vector<std::pair<a_base_class_ptr, a_routine_ptr>> virt_bases;
  if (supp) {
    for (a_base_class_ptr b = supp->direct_base_classes; b != nullptr; b = b->next_direct) {
      if (!b->is_virtual && b->type) {
        a_routine_ptr b_dtor = nullptr;
        LLVM_GEN_BE_CHECK(llvm_find_class_destructor(b->type, &b_dtor));
        if (b_dtor) direct_nv_bases.push_back({b, b_dtor});
      }
    }
    if (!is_subobject_dtor) {
      for (a_base_class_ptr b = supp->base_classes; b != nullptr; b = b->next) {
        if (b->is_virtual && b->type) {
          a_routine_ptr vb_dtor = nullptr;
          LLVM_GEN_BE_CHECK(llvm_find_class_destructor(b->type, &vb_dtor));
          if (vb_dtor) virt_bases.push_back({b, vb_dtor});
        }
      }
    }
  }

  // Forward construction order is: virtual bases -> direct non-virtual bases -> fields.
  // Therefore, reverse destruction order is: fields (reversed) -> direct non-virtual bases (reversed) -> virtual bases (reversed).
  std::vector<std::pair<llvm::Value*, a_routine_ptr>> ordered_cleanups;
  for (auto it = dtor_actions.rbegin(); it != dtor_actions.rend(); ++it) {
    ordered_cleanups.push_back(*it);
  }
  for (auto it = direct_nv_bases.rbegin(); it != direct_nv_bases.rend(); ++it) {
    llvm::Value* b_ptr = be_state->builder->CreateInBoundsGEP(
        i8_ty, obj_ptr, llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(it->first->offset)), "dtor.base.ptr");
    ordered_cleanups.push_back({b_ptr, it->second});
  }
  for (auto it = virt_bases.rbegin(); it != virt_bases.rend(); ++it) {
    llvm::Value* vb_ptr = be_state->builder->CreateInBoundsGEP(
        i8_ty, obj_ptr, llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(it->first->offset)), "dtor.vbase.ptr");
    ordered_cleanups.push_back({vb_ptr, it->second});
  }

  for (size_t i = 0; i < ordered_cleanups.size(); ++i) {
    size_t saved_depth = be_state->cleanup_stack.cleanups.size();
    for (size_t rem = ordered_cleanups.size(); rem > i + 1; --rem) {
      LLVM_GEN_BE_CHECK(llvm_emit_eh_cleanup(
          ordered_cleanups[rem - 1].second, ordered_cleanups[rem - 1].first));
    }
    a_routine_ptr dtor_r = ordered_cleanups[i].second;
    llvm::Value* sub_ptr = ordered_cleanups[i].first;
    const char* dtor_name = nullptr;
    LLVM_GEN_BE_CHECK(llvm_get_routine_symbol_name(dtor_r, &dtor_name));
    llvm::Function* dtor_fn = be_state->module->getFunction(dtor_name);
    if (!dtor_fn) {
      llvm::FunctionType* dtor_fty = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*be_state->context), {ptr_ty}, false);
      dtor_fn = llvm::Function::Create(
          dtor_fty, llvm::GlobalValue::ExternalLinkage, dtor_name, be_state->module.get());
    }
    llvm::BasicBlock* lpad_bb = nullptr;
    if (!dtor_fn->doesNotThrow()) {
      LLVM_GEN_BE_CHECK(llvm_get_or_create_current_unwind_dest(&lpad_bb));
    }
    if (lpad_bb) {
      llvm::BasicBlock* cont_bb = llvm::BasicBlock::Create(*be_state->context, "dtor.sub.cont", func);
      be_state->builder->CreateInvoke(dtor_fn, cont_bb, lpad_bb, {sub_ptr});
      be_state->builder->SetInsertPoint(cont_bb);
    } else {
      be_state->builder->CreateCall(dtor_fn, {sub_ptr});
    }
    while (be_state->cleanup_stack.cleanups.size() > saved_depth) {
      be_state->cleanup_stack.cleanups.pop_back();
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Synthesizes complete (`C1`/`D1`), base-subobject (`C2`/`D2`), and deleting (`D0` / MSVC `??_G`) constructor/destructor variants.
 * @details When an unlowered Itanium ABI constructor (`C2`) or destructor (`D2`) is defined without
 *          explicit front-end alias entries (`primary_ctor_or_dtor == nullptr`), emits the corresponding
 *          complete (`C1`/`D1`) and deleting (`D0`, calling class-specific `operator delete`, sized `_ZdlPvm`,
 *          or `_ZdlPv`) forwarding functions in the module. On MSVC targets, synthesizes the scalar deleting
 *          destructor (`??_G<Class>@@UEAAPEAXI@Z`) taking `(ptr this, i32 should_delete)`.
 * @param[in] routine The EDG constructor or destructor routine descriptor.
 * @param[in] base_fn The lowered LLVM Function for @p routine.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t llvm_emit_ctor_dtor_variants(
    a_routine_ptr routine,
    llvm::Function* base_fn) noexcept {
  if ((routine->special_kind != sfk_constructor &&
       routine->special_kind != sfk_destructor) ||
      routine->primary_ctor_or_dtor != nullptr) {
    return llvm_gen_be_error_t::ok;
  }

  a_type_ptr owner_class_tp = nullptr;
  if (routine->source_corresp.parent_scope &&
      routine->source_corresp.parent_scope->kind == sck_class_struct_union &&
      routine->source_corresp.parent_scope->variant.assoc_type) {
    owner_class_tp = skip_typerefs(routine->source_corresp.parent_scope->variant.assoc_type);
  }
  a_routine_ptr custom_op_del = nullptr;
  if (owner_class_tp) {
    LLVM_GEN_BE_CHECK(llvm_find_class_operator_delete(owner_class_tp, &custom_op_del));
  }
  const char* custom_del_name = nullptr;
  if (custom_op_del) {
    LLVM_GEN_BE_CHECK(llvm_get_routine_symbol_name(custom_op_del, &custom_del_name));
  }

  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  if (triple.isWindowsMSVCEnvironment()) {
    if (routine->special_kind == sfk_destructor) {
      std::string cls_name = (owner_class_tp && owner_class_tp->source_corresp.name)
                                 ? owner_class_tp->source_corresp.name
                                 : base_fn->getName().str();
      std::string sdel_name = "??_G" + cls_name + "@@UEAAPEAXI@Z";
      if (!be_state->module->getFunction(sdel_name)) {
        llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
        llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
        llvm::FunctionType* sdel_fty = llvm::FunctionType::get(ptr_ty, {ptr_ty, i32_ty}, false);
        llvm::Function* sdel_fn = llvm::Function::Create(
            sdel_fty, base_fn->getLinkage(), sdel_name, be_state->module.get());
        llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*be_state->context, "entry", sdel_fn);
        llvm::BasicBlock* del_bb = llvm::BasicBlock::Create(*be_state->context, "scalar.del", sdel_fn);
        llvm::BasicBlock* ret_bb = llvm::BasicBlock::Create(*be_state->context, "scalar.ret", sdel_fn);
        llvm::IRBuilder<> b(entry_bb);
        llvm::Value* this_arg = sdel_fn->getArg(0);
        llvm::Value* flags_arg = sdel_fn->getArg(1);
        b.CreateCall(base_fn, {this_arg});
        llvm::Value* do_del = b.CreateICmpNE(
            b.CreateAnd(flags_arg, llvm::ConstantInt::get(i32_ty, 1)),
            llvm::ConstantInt::get(i32_ty, 0));
        b.CreateCondBr(do_del, del_bb, ret_bb);

        b.SetInsertPoint(del_bb);
        const char* msvc_del_sym = custom_del_name ? custom_del_name : "??3@YAXPEAX@Z";
        llvm::FunctionType* del_fty = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*be_state->context), {ptr_ty}, false);
        llvm::FunctionCallee del_fn = be_state->module->getOrInsertFunction(msvc_del_sym, del_fty);
        b.CreateCall(del_fn, {this_arg});
        b.CreateBr(ret_bb);

        b.SetInsertPoint(ret_bb);
        b.CreateRet(this_arg);
      }

      std::string vdel_name = "??_E" + cls_name + "@@UEAAPEAXI@Z";
      if (!be_state->module->getFunction(vdel_name)) {
        llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
        llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
        llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
        llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
        llvm::FunctionType* vdel_fty = llvm::FunctionType::get(ptr_ty, {ptr_ty, i32_ty}, false);
        llvm::Function* vdel_fn = llvm::Function::Create(
            vdel_fty, base_fn->getLinkage(), vdel_name, be_state->module.get());
        llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*be_state->context, "entry", vdel_fn);
        llvm::BasicBlock* vec_bb = llvm::BasicBlock::Create(*be_state->context, "vec.cookie", vdel_fn);
        llvm::BasicBlock* vec_loop_bb = llvm::BasicBlock::Create(*be_state->context, "vec.loop", vdel_fn);
        llvm::BasicBlock* vec_body_bb = llvm::BasicBlock::Create(*be_state->context, "vec.body", vdel_fn);
        llvm::BasicBlock* vec_done_bb = llvm::BasicBlock::Create(*be_state->context, "vec.done", vdel_fn);
        llvm::BasicBlock* vec_free_bb = llvm::BasicBlock::Create(*be_state->context, "vec.free", vdel_fn);
        llvm::BasicBlock* scalar_bb = llvm::BasicBlock::Create(*be_state->context, "scalar.dtor", vdel_fn);
        llvm::BasicBlock* scalar_free_bb = llvm::BasicBlock::Create(*be_state->context, "scalar.free", vdel_fn);
        llvm::BasicBlock* ret_bb = llvm::BasicBlock::Create(*be_state->context, "vdel.ret", vdel_fn);

        llvm::IRBuilder<> b(entry_bb);
        llvm::Value* this_arg = vdel_fn->getArg(0);
        llvm::Value* flags_arg = vdel_fn->getArg(1);
        uint64_t elem_sz = (owner_class_tp && owner_class_tp->size > 0)
                               ? static_cast<uint64_t>(owner_class_tp->size)
                               : 8ULL;
        const char* msvc_del_sym = custom_del_name ? custom_del_name : "??3@YAXPEAX@Z";
        llvm::FunctionType* del_fty = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*be_state->context), {ptr_ty}, false);
        llvm::FunctionCallee del_fn = be_state->module->getOrInsertFunction(msvc_del_sym, del_fty);

        llvm::Value* is_vec = b.CreateICmpNE(
            b.CreateAnd(flags_arg, llvm::ConstantInt::get(i32_ty, 2)),
            llvm::ConstantInt::get(i32_ty, 0));
        b.CreateCondBr(is_vec, vec_bb, scalar_bb);

        b.SetInsertPoint(vec_bb);
        llvm::Value* cookie_ptr = b.CreateInBoundsGEP(
            i8_ty, this_arg, llvm::ConstantInt::getSigned(i64_ty, -8), "cookie.ptr");
        llvm::Value* count = b.CreateLoad(i64_ty, cookie_ptr, "cookie.count");
        b.CreateBr(vec_loop_bb);

        b.SetInsertPoint(vec_loop_bb);
        llvm::PHINode* idx_phi = b.CreatePHI(i64_ty, 2, "vec.idx");
        idx_phi->addIncoming(count, vec_bb);
        llvm::Value* has_more = b.CreateICmpUGT(idx_phi, llvm::ConstantInt::get(i64_ty, 0));
        b.CreateCondBr(has_more, vec_body_bb, vec_done_bb);

        b.SetInsertPoint(vec_body_bb);
        llvm::Value* next_idx = b.CreateSub(idx_phi, llvm::ConstantInt::get(i64_ty, 1));
        idx_phi->addIncoming(next_idx, vec_body_bb);
        llvm::Value* elem_off = b.CreateMul(next_idx, llvm::ConstantInt::get(i64_ty, elem_sz));
        llvm::Value* elem_ptr = b.CreateInBoundsGEP(i8_ty, this_arg, elem_off, "vec.elem");
        b.CreateCall(base_fn, {elem_ptr});
        b.CreateBr(vec_loop_bb);

        b.SetInsertPoint(vec_done_bb);
        llvm::Value* do_vec_del = b.CreateICmpNE(
            b.CreateAnd(flags_arg, llvm::ConstantInt::get(i32_ty, 1)),
            llvm::ConstantInt::get(i32_ty, 0));
        b.CreateCondBr(do_vec_del, vec_free_bb, ret_bb);

        b.SetInsertPoint(vec_free_bb);
        b.CreateCall(del_fn, {cookie_ptr});
        b.CreateBr(ret_bb);

        b.SetInsertPoint(scalar_bb);
        b.CreateCall(base_fn, {this_arg});
        llvm::Value* do_scalar_del = b.CreateICmpNE(
            b.CreateAnd(flags_arg, llvm::ConstantInt::get(i32_ty, 1)),
            llvm::ConstantInt::get(i32_ty, 0));
        b.CreateCondBr(do_scalar_del, scalar_free_bb, ret_bb);

        b.SetInsertPoint(scalar_free_bb);
        b.CreateCall(del_fn, {this_arg});
        b.CreateBr(ret_bb);

        b.SetInsertPoint(ret_bb);
        llvm::PHINode* ret_phi = b.CreatePHI(ptr_ty, 4, "vdel.res");
        ret_phi->addIncoming(cookie_ptr, vec_done_bb);
        ret_phi->addIncoming(cookie_ptr, vec_free_bb);
        ret_phi->addIncoming(this_arg, scalar_bb);
        ret_phi->addIncoming(this_arg, scalar_free_bb);
        b.CreateRet(ret_phi);
      }
    }
    return llvm_gen_be_error_t::ok;
  }
  std::string base_name = base_fn->getName().str();
  bool has_vbases = (owner_class_tp && owner_class_tp->variant.class_struct_union.any_virtual_base_classes);
  size_t c2_pos = base_name.find("C2E");
  if (routine->special_kind == sfk_constructor && c2_pos != std::string::npos) {
    std::string c1_name = base_name;
    c1_name[c2_pos + 1] = '1';
    if (!be_state->module->getFunction(c1_name)) {
      bool c2_has_vtt = false;
      if (has_vbases) {
        a_scope_ptr r_sc = nullptr;
        LLVM_GEN_BE_CHECK(get_scope_for_routine_definition(routine, &r_sc));
        a_variable_ptr p0 = r_sc->variant.routine.parameters;
        if (p0 && p0->next && p0->next->source_corresp.name &&
            std::strstr(p0->next->source_corresp.name, "vtt") != nullptr &&
            base_fn->arg_size() >= 2) {
          c2_has_vtt = true;
        }
      }
      llvm::FunctionType* c1_fty = base_fn->getFunctionType();
      llvm::Constant* vtt_const = nullptr;
      if (c2_has_vtt) {
        LLVM_GEN_BE_CHECK(llvm_emit_vtt(owner_class_tp, &vtt_const));
        std::vector<llvm::Type*> c1_params;
        c1_params.push_back(base_fn->getFunctionType()->getParamType(0));
        for (unsigned pi = 2; pi < base_fn->getFunctionType()->getNumParams(); ++pi) {
          c1_params.push_back(base_fn->getFunctionType()->getParamType(pi));
        }
        c1_fty = llvm::FunctionType::get(base_fn->getReturnType(), c1_params, base_fn->isVarArg());
      }
      llvm::Function* c1_fn = llvm::Function::Create(
          c1_fty,
          base_fn->getLinkage(),
          c1_name,
          be_state->module.get());
      llvm::BasicBlock* bb = llvm::BasicBlock::Create(*be_state->context, "entry", c1_fn);
      llvm::IRBuilder<> b(bb);
      std::vector<llvm::Value*> args;
      unsigned ai = 0;
      for (llvm::Argument& a : c1_fn->args()) {
        args.push_back(&a);
        if (ai == 0 && c2_has_vtt) {
          args.push_back(vtt_const);
        }
        ++ai;
      }
      llvm::CallInst* call = b.CreateCall(base_fn, args);
      call->setTailCall(true);
      if (c1_fn->getReturnType()->isVoidTy()) {
        b.CreateRetVoid();
      } else {
        b.CreateRet(call);
      }
    }
    return llvm_gen_be_error_t::ok;
  }
  size_t d2_pos = base_name.find("D2E");
  if (routine->special_kind == sfk_destructor && d2_pos != std::string::npos) {
    bool d2_has_vtt = (has_vbases && base_fn->arg_size() == 2);
    llvm::Constant* d_vtt_const = nullptr;
    llvm::FunctionType* d1_fty = base_fn->getFunctionType();
    if (d2_has_vtt) {
      LLVM_GEN_BE_CHECK(llvm_emit_vtt(owner_class_tp, &d_vtt_const));
      d1_fty = llvm::FunctionType::get(
          base_fn->getReturnType(), {base_fn->getFunctionType()->getParamType(0)}, false);
    }
    std::string d1_name = base_name;
    d1_name[d2_pos + 1] = '1';
    if (!be_state->module->getFunction(d1_name)) {
      llvm::Function* d1_fn = llvm::Function::Create(
          d1_fty,
          base_fn->getLinkage(),
          d1_name,
          be_state->module.get());
      llvm::BasicBlock* bb = llvm::BasicBlock::Create(*be_state->context, "entry", d1_fn);
      llvm::IRBuilder<> b(bb);
      std::vector<llvm::Value*> args;
      for (llvm::Argument& a : d1_fn->args()) {
        args.push_back(&a);
      }
      if (d2_has_vtt) {
        args.push_back(d_vtt_const);
      }
      llvm::CallInst* call = b.CreateCall(base_fn, args);
      call->setTailCall(true);
      if (d1_fn->getReturnType()->isVoidTy()) {
        b.CreateRetVoid();
      } else {
        b.CreateRet(call);
      }
    }
    std::string d0_name = base_name;
    d0_name[d2_pos + 1] = '0';
    if (!be_state->module->getFunction(d0_name) && base_fn->arg_size() > 0) {
      // Itanium ABI §3.1.4: D0 deleting destructor always returns void (even on ARM64 where D1/D2 return this)
      llvm::FunctionType* d0_fty = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*be_state->context),
          std::vector<llvm::Type*>(d1_fty->param_begin(), d1_fty->param_end()),
          false);
      llvm::Function* d0_fn = llvm::Function::Create(
          d0_fty,
          base_fn->getLinkage(),
          d0_name,
          be_state->module.get());
      llvm::BasicBlock* bb = llvm::BasicBlock::Create(*be_state->context, "entry", d0_fn);
      llvm::IRBuilder<> b(bb);
      std::vector<llvm::Value*> args;
      for (llvm::Argument& a : d0_fn->args()) {
        args.push_back(&a);
      }
      if (d2_has_vtt) {
        args.push_back(d_vtt_const);
      }
      b.CreateCall(base_fn, args);
      llvm::Type* void_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
      llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
      if (custom_del_name) {
        llvm::FunctionType* del_fty = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*be_state->context), {void_ptr_ty}, false);
        llvm::FunctionCallee del_fn = be_state->module->getOrInsertFunction(custom_del_name, del_fty);
        b.CreateCall(del_fn, {d0_fn->getArg(0)});
      } else if (owner_class_tp) {
        llvm::FunctionType* sdel_fty = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*be_state->context), {void_ptr_ty, i64_ty}, false);
        llvm::FunctionCallee sdel_fn = be_state->module->getOrInsertFunction("_ZdlPvm", sdel_fty);
        b.CreateCall(sdel_fn, {d0_fn->getArg(0), llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(owner_class_tp->size))});
      } else {
        llvm::FunctionType* del_fty = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*be_state->context), {void_ptr_ty}, false);
        llvm::FunctionCallee del_fn = be_state->module->getOrInsertFunction("_ZdlPv", del_fty);
        b.CreateCall(del_fn, {d0_fn->getArg(0)});
      }
      b.CreateRetVoid();
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Computes return value `sret` and coerced-type information across all 11 supported target ABIs.
 * @details Inspects the active module's target triple and dispatches to the matching target ABI
 *          return classification routine (SysV x86_64, Win64, AAPCS64, x86_32, ARM32, RISC-V,
 *          LoongArch64, MIPS/MIPS64, PPC64, SystemZ s390x, SPARC V8/V9, or WebAssembly).
 * @param[in] ret_ty Pointer to the EDG return type.
 * @param[in] is_vectorcall True when the routine uses `X86_VectorCall` calling convention.
 * @param[out] out_sret Receives `true` if the return value requires a hidden `sret` pointer parameter.
 * @param[out] out_coerced_ty Receives the coerced return `llvm::Type*` (or `nullptr` if uncoerced).
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_compute_target_return_info(
    a_type_ptr ret_ty,
    bool is_vectorcall,
    bool* out_sret,
    llvm::Type** out_coerced_ty) noexcept {
  if (!out_sret || !out_coerced_ty) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  *out_sret = false;
  *out_coerced_ty = nullptr;

  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;

  if (triple.isOSWindows() && triple.getArch() == llvm::Triple::x86_64) {
    win64_arg_info_t ret_info = {};
    err = compute_win64_return_info(ret_ty, is_vectorcall, &ret_info, out_sret);
    *out_coerced_ty = ret_info.hva_elem_ty;
  } else if (triple.isAArch64()) {
    aapcs64_arg_info_t ret_info = {};
    err = compute_aapcs64_return_info(ret_ty, &ret_info, out_sret);
    *out_coerced_ty = ret_info.hfa_hva_elem_ty;
  } else if (triple.getArch() == llvm::Triple::x86) {
    x86_32_arg_info_t ret_info = {};
    err = classify_x86_32_argument(ret_ty, &ret_info);
    *out_sret = ret_info.is_indirect;
    *out_coerced_ty = ret_info.coerced_type;
  } else if (triple.getArch() == llvm::Triple::arm || triple.getArch() == llvm::Triple::thumb) {
    arm32_arg_info_t ret_info = {};
    err = classify_arm32_argument(ret_ty, &ret_info);
    *out_sret = (ret_info.is_indirect || (!ret_info.is_hfa && ret_info.size > 4));
    *out_coerced_ty = ret_info.coerced_type;
  } else if (triple.isRISCV64() || triple.isRISCV32()) {
    riscv64_arg_info_t ret_info = {};
    err = compute_riscv64_return_info(ret_ty, &ret_info, out_sret);
    *out_coerced_ty = ret_info.coerced_type;
  } else if (triple.getArch() == llvm::Triple::loongarch64 || triple.getArch() == llvm::Triple::loongarch32) {
    loongarch64_arg_info_t ret_info = {};
    err = compute_loongarch64_return_info(ret_ty, &ret_info, out_sret);
    *out_coerced_ty = ret_info.coerced_type;
  } else if (triple.isMIPS()) {
    mips_arg_info_t ret_info = {};
    err = compute_mips_return_info(ret_ty, false, triple.isMIPS64(), &ret_info, out_sret);
    *out_coerced_ty = ret_info.coerced_type;
  } else if (triple.isPPC64() || triple.isPPC32()) {
    ppc64_arg_info_t ret_info = {};
    err = compute_ppc64_return_info(ret_ty, triple.isLittleEndian(), &ret_info, out_sret);
    *out_coerced_ty = ret_info.coerced_type;
  } else if (triple.getArch() == llvm::Triple::systemz) {
    s390x_arg_info_t ret_info = {};
    err = compute_s390x_return_info(ret_ty, &ret_info, out_sret);
    *out_coerced_ty = ret_info.coerced_type;
  } else if (triple.getArch() == llvm::Triple::sparc || triple.getArch() == llvm::Triple::sparcv9 ||
             triple.getArch() == llvm::Triple::sparcel) {
    sparc_arg_info_t ret_info = {};
    err = compute_sparc_return_info(ret_ty, triple.getArch() == llvm::Triple::sparcv9, &ret_info, out_sret);
    *out_coerced_ty = ret_info.coerced_type;
  } else if (triple.isWasm()) {
    wasm_arg_info_t ret_info = {};
    err = classify_wasm_argument(ret_ty, &ret_info);
    *out_sret = ret_info.is_indirect;
  } else {
    x86_64_abi_arg_info_t ret_info = {};
    err = compute_sysv_return_info(ret_ty, &ret_info, out_sret);
  }
  return err;
}

/**
 * @brief Lowers a single EDG routine definition body into LLVM IR.
 * @details Allocates parameter and local variable stack slots, unpacks ABI-coerced parameters,
 *          lowers constructor initializers and body statements, and emits Itanium C1/D1/D0 aliases.
 * @param[in] routine The EDG routine to lower.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_function_body(a_routine_ptr routine) noexcept {
    if (!routine || !routine->source_corresp.name) return llvm_gen_be_error_t::ok;
    if (!routine->function_def_number) return llvm_gen_be_error_t::ok; // No body

    llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
    if (!func) return llvm_gen_be_error_t::ok;

    llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*be_state->context, "entry", func);
    be_state->builder->SetInsertPoint(entry_bb);

    // Look up the function definition
    a_scope_ptr func_scope = nullptr;
    LLVM_GEN_BE_CHECK(get_scope_for_routine_definition(routine, &func_scope));
    if (!func_scope) return llvm_gen_be_error_t::ok;

    llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;
    llvm::DISubprogram* di_subprogram = nullptr;
    if (be_state->dbg_state) {
        err = create_di_subprogram(be_state->dbg_state, routine, func, &di_subprogram);
        if (err != llvm_gen_be_error_t::ok) return err;
    }

    // Clear local variables and labels for the new function scope
    be_state->local_vars.clear();
    be_state->vla_ptr_slots.clear();
    be_state->label_blocks.clear();

    for (a_local_static_variable_init_ptr lsvip = func_scope->local_static_variable_inits;
         lsvip != nullptr; lsvip = lsvip->next) {
      if (lsvip->variable && lsvip->variable->init_kind == initk_function_local) {
        lsvip->variable->init_kind = lsvip->init_kind;
        lsvip->variable->initializer = lsvip->initializer;
      }
    }

    for (a_pragma_ptr p = func_scope->pragmas; p != nullptr; p = p->next) {
      if (p->entity.ptr && p->pragma_text) {
        be_state->omp_pragmas[reinterpret_cast<a_statement_ptr>(p->entity.ptr)] = p->pragma_text;
      }
    }
    if (il_header.primary_scope) {
      for (a_pragma_ptr p = il_header.primary_scope->pragmas; p != nullptr; p = p->next) {
        if (p->entity.ptr && p->pragma_text) {
          be_state->omp_pragmas[reinterpret_cast<a_statement_ptr>(p->entity.ptr)] = p->pragma_text;
        }
      }
    }
    if (be_state->fopenmp_enabled && func_scope->assoc_block) {
      std::unordered_set<std::string> claimed_omp_lines;
      LLVM_GEN_BE_CHECK(scan_omp_pragmas_in_stmt_tree(func_scope->assoc_block, &claimed_omp_lines));
    }

    for (a_label_ptr label = func_scope->labels; label != nullptr; label = label->next) {
      be_state->label_blocks[label] = llvm::BasicBlock::Create(*be_state->context, "label", func);
    }

    // Check if we have backend-synthesized sret across all 11 target ABIs
    bool edg_ret_as_param = false;
#if DO_IL_LOWERING
    if (routine->type->kind == tk_routine &&
        routine->type->variant.routine.extra_info != nullptr &&
        routine->type->variant.routine.extra_info->value_returned_as_parameter) {
      edg_ret_as_param = true;
    }
#endif
    bool sret = !edg_ret_as_param && func->hasStructRetAttr();
    if (!edg_ret_as_param && !sret && routine->type->kind == tk_routine) {
      bool is_vectorcall = (func->getCallingConv() == llvm::CallingConv::X86_VectorCall);
      llvm::Type* coerced_ret_ty = nullptr;
      err = llvm_compute_target_return_info(
          routine->type->variant.routine.return_type, is_vectorcall, &sret, &coerced_ret_ty);
      if (err != llvm_gen_be_error_t::ok) return err;
    }

    unsigned arg_idx = sret ? 1 : 0;
    if (sret) {
       func->getArg(0)->setName("sret_ptr");
    }

    // Allocate parameters and bind arguments
    for (a_variable_ptr param = func_scope->variant.routine.parameters; param != nullptr; param = param->next) {
      if (arg_idx < func->arg_size()) {
        llvm::Argument* arg = func->getArg(arg_idx++);
        if (param->source_corresp.name) {
          arg->setName(param->source_corresp.name);
        }

        llvm::Type* param_ty = nullptr;
        err = get_llvm_type(param->type, &param_ty);
        if (err != llvm_gen_be_error_t::ok) return err;
        llvm::AllocaInst* alloca = be_state->builder->CreateAlloca(param_ty, nullptr, param->source_corresp.name ? std::string(param->source_corresp.name) + ".addr" : "");
        
        // Unpacking logic for 1.3.4
        // If the LLVM arg type differs from the EDG param type (due to register packing or byval/indirect ptr)
        // we must cast or load it.
        if (arg->hasByValAttr() || (arg->getType()->isPointerTy() && !param_ty->isPointerTy())) {
           llvm::Value* cast_arg = be_state->builder->CreatePointerCast(arg, llvm::PointerType::getUnqual(*be_state->context));
           llvm::Value* val = be_state->builder->CreateLoad(param_ty, cast_arg);
           be_state->builder->CreateStore(val, alloca);
        } else if (arg->getType() != param_ty) {
           // Direct register packed or multi-register split struct
           llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
           llvm::Value* dst_ptr = be_state->builder->CreatePointerCast(alloca, ptr_ty);
           const llvm::DataLayout& dl = be_state->module->getDataLayout();
           uint64_t unpacked_bytes = dl.getTypeAllocSize(arg->getType());
           uint64_t total_param_bytes = dl.getTypeAllocSize(param_ty);
           if (unpacked_bytes > total_param_bytes) {
             llvm::AllocaInst* tmp_coerced = be_state->builder->CreateAlloca(arg->getType(), nullptr, "arg.coerced.tmp");
             be_state->builder->CreateStore(arg, tmp_coerced);
             be_state->builder->CreateMemCpy(dst_ptr, llvm::MaybeAlign(1), tmp_coerced, llvm::MaybeAlign(1), total_param_bytes);
           } else {
             be_state->builder->CreateStore(arg, dst_ptr);
           }

           unsigned remaining_edg_params = 0;
           for (a_variable_ptr rem = param->next; rem != nullptr; rem = rem->next) {
             remaining_edg_params++;
           }
           while (param_ty->isAggregateType() &&
                  unpacked_bytes < total_param_bytes &&
                  (func->arg_size() - arg_idx) > remaining_edg_params) {
             llvm::Argument* next_arg = func->getArg(arg_idx++);
             llvm::Value* piece_ptr = be_state->builder->CreateInBoundsGEP(
                 llvm::Type::getInt8Ty(*be_state->context), dst_ptr,
                 llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), unpacked_bytes), "arg.piece.ptr");
             be_state->builder->CreateStore(next_arg, piece_ptr);
             unpacked_bytes += dl.getTypeAllocSize(next_arg->getType());
           }
        } else {
           be_state->builder->CreateStore(arg, alloca);
        }
        
        be_state->local_vars[param] = alloca;
        
        if (be_state->dbg_state) {
            err = emit_dbg_declare_for_variable(be_state->dbg_state, param, alloca);
            if (err != llvm_gen_be_error_t::ok) return err;
        }
      }
    }

    // Allocate local variables in function scope and nested block scopes (e.g. for-init scopes)
    std::vector<a_scope_ptr> scope_worklist = {func_scope};
    for (size_t s_idx = 0; s_idx < scope_worklist.size(); ++s_idx) {
      a_scope_ptr cur_sc = scope_worklist[s_idx];
      for (a_variable_ptr lvar = cur_sc->nonstatic_variables; lvar != nullptr; lvar = lvar->next) {
        llvm::Type* lvar_ty = nullptr;
        err = get_llvm_type(lvar->type, &lvar_ty);
        if (err != llvm_gen_be_error_t::ok) return err;
        llvm::AllocaInst* alloca = be_state->builder->CreateAlloca(
            lvar_ty, nullptr, lvar->source_corresp.name ? lvar->source_corresp.name : "");
        be_state->local_vars[lvar] = alloca;

        if (be_state->dbg_state) {
          err = emit_dbg_declare_for_variable(be_state->dbg_state, lvar, alloca);
          if (err != llvm_gen_be_error_t::ok) return err;
        }
      }
      for (a_variable_ptr svar = cur_sc->variables; svar != nullptr; svar = svar->next) {
        if (svar->storage_class == sc_static) {
          a_scope_ptr saved_ps = il_header.primary_scope;
          il_header.primary_scope = cur_sc;
          err = llvm_lower_global_variable(svar);
          il_header.primary_scope = saved_ps;
          if (err != llvm_gen_be_error_t::ok) return err;
        }
      }
      for (a_scope_ptr sub_sc = cur_sc->scopes; sub_sc != nullptr; sub_sc = sub_sc->next) {
        if (!sub_sc->is_expansion_prototype) {
          scope_worklist.push_back(sub_sc);
        }
      }
    }

    if (func_scope->variant.routine.constructor_inits) {
        llvm_gen_be_error_t init_err = llvm_lower_constructor_inits(routine, func_scope, func);
        if (init_err != llvm_gen_be_error_t::ok) return init_err;
    }

    llvm_gen_be_error_t vptr_err = llvm_emit_ctor_dtor_vptr_init(routine, func_scope);
    if (vptr_err != llvm_gen_be_error_t::ok) return vptr_err;

    bool saved_ctor_dtor_flag = be_state->current_routine_is_ctor_or_dtor;
    a_routine_ptr saved_edg_routine = be_state->current_edg_routine;
    a_scope_ptr saved_func_scope = be_state->current_function_scope;
    be_state->current_edg_routine = routine;
    be_state->current_function_scope = func_scope;
    be_state->current_routine_is_ctor_or_dtor =
        (routine->special_kind == sfk_constructor ||
         routine->special_kind == sfk_destructor ||
         func_scope->variant.routine.constructor_inits != nullptr);

    llvm::BasicBlock* exc_spec_lpad_bb = nullptr;
    a_type_ptr rtype = skip_typerefs(routine->type);
    if (rtype->variant.routine.extra_info && rtype->variant.routine.extra_info->exception_specification) {
        an_exception_specification_ptr spec = rtype->variant.routine.extra_info->exception_specification;
        if (!spec->throw_any) {
            exc_spec_lpad_bb = llvm::BasicBlock::Create(*be_state->context, "exc_spec.lpad", func);
            be_state->current_landing_pads.push_back(exc_spec_lpad_bb);
        }
    }

    if (func_scope->assoc_block) {
      llvm_gen_be_error_t err = llvm_lower_statement(func_scope->assoc_block);
      if (err != llvm_gen_be_error_t::ok) {
        be_state->current_routine_is_ctor_or_dtor = saved_ctor_dtor_flag;
        be_state->current_edg_routine = saved_edg_routine;
        be_state->current_function_scope = saved_func_scope;
        return err;
      }
    }
    LLVM_GEN_BE_CHECK(llvm_lower_destructor_cleanups(routine, func_scope, func));
    if (be_state->current_coro_cleanup_bb &&
        be_state->current_coro_cleanup_bb->getParent() == func) {
      if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
        be_state->builder->CreateBr(be_state->current_coro_cleanup_bb);
      }
      be_state->current_coro_handle = nullptr;
      be_state->current_coro_promise = nullptr;
      be_state->current_coro_gro_slot = nullptr;
      be_state->current_coro_cleanup_bb = nullptr;
      be_state->current_coro_end_bb = nullptr;
      be_state->current_coro_suspend_bb = nullptr;
      be_state->current_coro_final_suspend_bb = nullptr;
      be_state->current_coro_final_await_expr = nullptr;
    }
    be_state->current_routine_is_ctor_or_dtor = saved_ctor_dtor_flag;
    be_state->current_edg_routine = saved_edg_routine;
    be_state->current_function_scope = saved_func_scope;

    if (exc_spec_lpad_bb) {
        be_state->current_landing_pads.pop_back();
        LLVM_GEN_BE_CHECK(llvm_emit_exception_specification_lpad(routine, exc_spec_lpad_bb));
    }

    if (!be_state->escaped_allocas.empty()) {
        llvm::FunctionCallee escape_fn = be_state->module->getOrInsertFunction(
            "llvm.localescape", llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {}, true));
        
        llvm::BasicBlock& entry_bb = func->getEntryBlock();
        llvm::Instruction* terminator = EDG_LLVM_GET_TERMINATOR(&entry_bb);
        if (terminator) {
            be_state->builder->SetInsertPoint(terminator);
        } else {
            be_state->builder->SetInsertPoint(&entry_bb);
        }
        be_state->builder->CreateCall(escape_fn, be_state->escaped_allocas);
        
        be_state->escaped_allocas.clear();
        be_state->escaped_var_indices.clear();
    }

    // Ensure all blocks have a terminator
    for (llvm::BasicBlock& bb : *func) {
      if (!EDG_LLVM_GET_TERMINATOR(&bb)) {
        be_state->builder->SetInsertPoint(&bb);
        if (func->getReturnType()->isVoidTy()) {
          be_state->builder->CreateRetVoid();
        } else if ((routine->special_kind == sfk_constructor ||
                    routine->special_kind == sfk_destructor) &&
                   func->getReturnType()->isPointerTy() &&
                   func->arg_size() > 0 &&
                   func->getArg(0)->getType() == func->getReturnType()) {
          be_state->builder->CreateRet(func->getArg(0));
        } else {
          if (be_state->enable_ubsan_checks) {
            llvm::FunctionType* mr_fty = llvm::FunctionType::get(
                llvm::Type::getVoidTy(*be_state->context),
                {llvm::PointerType::getUnqual(*be_state->context)}, false);
            llvm::FunctionCallee mr_fn = be_state->module->getOrInsertFunction(
                "__ubsan_handle_missing_return", mr_fty);
            be_state->builder->CreateCall(
                mr_fn, {llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context))});
          }
          be_state->builder->CreateUnreachable();
        }
      }
    }
    
    if (be_state->dbg_state) {
      llvm_gen_be_error_t err = pop_lexical_block(be_state->dbg_state);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
    std::string err_str;
    llvm::raw_string_ostream os{err_str};
    bool invalid_ir = llvm::verifyFunction(*func, &os);
    if (invalid_ir) {
      os.flush();
      return llvm_gen_be_set_error(
          &be_state->err_context, llvm_gen_be_error_t::verification_failure,
          __FILE__, __LINE__, 0, "Function verification failed (%s): %s",
          func->getName().str().c_str(), err_str.c_str());
    }
    LLVM_GEN_BE_CHECK(llvm_emit_ctor_dtor_variants(routine, func));
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Retrieves or creates the external hidden `@__dso_handle` global variable for Itanium ABI destructor registration.
 * @details Used by `__cxa_atexit` and `__cxa_thread_atexit` calls.
 * @param[out] out_dso_handle Receives the `@__dso_handle` `llvm::GlobalVariable*`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t get_or_create_dso_handle(
    llvm::GlobalVariable** out_dso_handle) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  llvm::GlobalVariable* dso_handle = be_state->module->getGlobalVariable("__dso_handle");
  if (!dso_handle) {
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), i8_ty, false, llvm::GlobalValue::ExternalLinkage, nullptr, "__dso_handle", &dso_handle));
    dso_handle->setVisibility(llvm::GlobalValue::HiddenVisibility);
  }
  *out_dso_handle = dso_handle;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Synthesizes the `__tls_init` function body and optional `_ZTW<var>` wrappers for thread-local variables.
 * @details Emits dynamic initialization guarded by a thread-local `i8` guard variable `__tls_guard`,
 *          registers non-trivial destructors via `__cxa_thread_atexit` (Itanium) or
 *          `_register_thread_local_exe_atexit_callback` (MSVC), and synthesizes Itanium `_ZTW` wrappers.
 * @param[in,out] target_func The `__tls_init` `llvm::Function` to populate if currently empty.
 * @param[in] is_msvc True when targeting a Windows MSVC environment.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t synthesize_tls_dynamic_init(
    llvm::Function* target_func,
    bool is_msvc) noexcept {
  llvm::Type* void_ty = llvm::Type::getVoidTy(*be_state->context);
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);

  if (!target_func->empty()) {
    return llvm_gen_be_error_t::ok;
  }

  llvm::GlobalVariable* tls_guard = be_state->module->getGlobalVariable("__tls_guard", true);
  if (!tls_guard) {
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), i8_ty, false, llvm::GlobalValue::InternalLinkage, llvm::ConstantInt::get(i8_ty, 0), "__tls_guard", &tls_guard, llvm::GlobalValue::GeneralDynamicTLSModel));
  }
  llvm::BasicBlock* t_entry = llvm::BasicBlock::Create(*be_state->context, "entry", target_func);
  llvm::BasicBlock* t_init = llvm::BasicBlock::Create(*be_state->context, "init", target_func);
  llvm::BasicBlock* t_done = llvm::BasicBlock::Create(*be_state->context, "done", target_func);
  llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
  be_state->builder->SetInsertPoint(t_entry);
  llvm::Value* g_val = be_state->builder->CreateLoad(i8_ty, tls_guard, "tls.guard.val");
  llvm::Value* is_zero = be_state->builder->CreateICmpEQ(g_val, llvm::ConstantInt::get(i8_ty, 0));
  be_state->builder->CreateCondBr(is_zero, t_init, t_done);

  be_state->builder->SetInsertPoint(t_init);
  be_state->builder->CreateStore(llvm::ConstantInt::get(i8_ty, 1), tls_guard);
  for (const GlobalDynamicInitEntry& entry : be_state->global_dynamic_inits) {
    if (!entry.var->is_thread_local) continue;
    std::string vname;
    LLVM_GEN_BE_CHECK(llvm_get_mangled_variable_name(entry.var, &vname));
    llvm::GlobalVariable* tls_gv = be_state->module->getNamedGlobal(vname);
    llvm_gen_be_error_t init_err = llvm_emit_dynamic_init(entry.dip, tls_gv, tls_gv->getValueType());
    if (init_err != llvm_gen_be_error_t::ok) return init_err;
    if (entry.dip->destructor) {
      const char* dtor_name = entry.dip->destructor->source_corresp.name
                                  ? entry.dip->destructor->source_corresp.name
                                  : "dtor";
      llvm::FunctionType* dtor_fty = llvm::FunctionType::get(void_ty, {ptr_ty}, false);
      llvm::FunctionCallee dtor_callee = be_state->module->getOrInsertFunction(dtor_name, dtor_fty);
      if (is_msvc) {
        llvm::FunctionType* tlreg_ty = llvm::FunctionType::get(i32_ty, {ptr_ty, ptr_ty}, false);
        llvm::FunctionCallee tlreg_fn = be_state->module->getOrInsertFunction(
            "_register_thread_local_exe_atexit_callback", tlreg_ty);
        be_state->builder->CreateCall(tlreg_fn, {dtor_callee.getCallee(), tls_gv});
      } else {
        llvm::FunctionType* atexit_ty = llvm::FunctionType::get(i32_ty, {ptr_ty, ptr_ty, ptr_ty}, false);
        llvm::FunctionCallee cxa_t_atexit = be_state->module->getOrInsertFunction("__cxa_thread_atexit", atexit_ty);
        llvm::GlobalVariable* dso_handle = nullptr;
        LLVM_GEN_BE_CHECK(get_or_create_dso_handle(&dso_handle));
        be_state->builder->CreateCall(cxa_t_atexit, {dtor_callee.getCallee(), tls_gv, dso_handle});
      }
    }
    if (!is_msvc) {
      std::string ztw_name = (vname.rfind("_Z", 0) == 0)
                                 ? ("_ZTW" + vname.substr(2))
                                 : ("_ZTW" + std::to_string(vname.size()) + vname);
      if (!be_state->module->getFunction(ztw_name)) {
        llvm::FunctionType* ztw_fty = llvm::FunctionType::get(ptr_ty, false);
        llvm::Function* ztw_fn = llvm::Function::Create(
            ztw_fty, llvm::GlobalValue::WeakODRLinkage, ztw_name, be_state->module.get());
        llvm::BasicBlock* ztw_bb = llvm::BasicBlock::Create(*be_state->context, "entry", ztw_fn);
        llvm::IRBuilder<> ztw_builder(ztw_bb);
        ztw_builder.CreateCall(target_func);
        ztw_builder.CreateRet(tls_gv);
      }
    }
  }
  be_state->builder->CreateBr(t_done);
  be_state->builder->SetInsertPoint(t_done);
  be_state->builder->CreateRetVoid();
  be_state->builder->restoreIP(saved_ip);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a single routine definition (including ctor/dtor aliases, TLS wrappers, and thunks).
 * @details Emits the body for a routine in the current translation unit.
 * @param[in] routine The EDG routine to lower.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t lower_single_routine_definition(a_routine_ptr routine) noexcept {
  if (routine->is_prototype_instantiation || routine->is_consteval) {
    return llvm_gen_be_error_t::ok;
  }
#if DO_IL_LOWERING && IA64_ABI
  if (routine->primary_ctor_or_dtor) {
    // This is a C1/C2/D0/D1/D2 variant
    if (routine->is_alias_entry) {
      if (routine->compiler_generated &&
          !routine->primary_ctor_or_dtor->defined &&
          routine->primary_ctor_or_dtor->function_def_number == 0) {
        return llvm_gen_be_error_t::ok;
      }
      llvm::Function* target_func = be_state->module->getFunction(routine->primary_ctor_or_dtor->source_corresp.name);
      llvm::Function* alias_func = be_state->module->getFunction(routine->source_corresp.name);
      if (target_func && alias_func && alias_func->empty()) {
        alias_func->setLinkage(target_func->getLinkage());
        llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*be_state->context, "entry", alias_func);
        be_state->builder->SetInsertPoint(entry_bb);
        std::vector<llvm::Value*> args;
        for (auto& arg : alias_func->args()) {
          args.push_back(&arg);
        }
        llvm::CallInst* call = be_state->builder->CreateCall(target_func, args);
        call->setTailCall(true);
        
        if (routine->ctor_dtor_kind == cdk_deleting) {
          llvm::Type* void_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
          llvm::FunctionType* del_fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {void_ptr_ty}, false);
          llvm::FunctionCallee delete_func = be_state->module->getOrInsertFunction("_ZdlPv", del_fty);
          if (alias_func->arg_size() > 0) {
            llvm::Value* this_arg = alias_func->getArg(0);
            be_state->builder->CreateCall(delete_func, this_arg);
          }
        }
        if (alias_func->getReturnType()->isVoidTy()) {
          be_state->builder->CreateRetVoid();
        } else {
          be_state->builder->CreateRet(call);
        }
      }
      return llvm_gen_be_error_t::ok;
    }
    return llvm_lower_function_body(routine);
  }

  if (routine->is_tls_init_alias) {
    llvm::Function* alias_func = be_state->module->getFunction(routine->source_corresp.name);
    if (alias_func && alias_func->empty()) {
      llvm::Type* void_ty = llvm::Type::getVoidTy(*be_state->context);
      llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
      llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
      llvm::Function* target_func = be_state->module->getFunction("__tls_init");
      if (!target_func) {
        llvm::FunctionType* void_fn_ty = llvm::FunctionType::get(void_ty, false);
        target_func = llvm::Function::Create(void_fn_ty, llvm::GlobalValue::ExternalLinkage, "__tls_init", be_state->module.get());
      }
      llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());

      llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*be_state->context, "entry", alias_func);
      be_state->builder->SetInsertPoint(entry_bb);
      be_state->builder->CreateCall(target_func);
      
      if (triple.isWindowsMSVCEnvironment()) {
        llvm::FunctionType* dtor_ty = llvm::FunctionType::get(
            i32_ty, {ptr_ty, i32_ty, ptr_ty}, false);

        // Register TLS callback in .CRT$XDU section and check DLL_PROCESS_ATTACH (1) / DLL_THREAD_ATTACH (2)
        llvm::Function* tls_callback = llvm::Function::Create(
            dtor_ty, llvm::GlobalValue::InternalLinkage, "__tls_callback", be_state->module.get());
        llvm::BasicBlock* cb_entry = llvm::BasicBlock::Create(*be_state->context, "entry", tls_callback);
        llvm::BasicBlock* cb_attach = llvm::BasicBlock::Create(*be_state->context, "attach", tls_callback);
        llvm::BasicBlock* cb_exit = llvm::BasicBlock::Create(*be_state->context, "exit", tls_callback);
        llvm::IRBuilder<> cb_builder(cb_entry);
        llvm::Value* reason = tls_callback->getArg(1);
        llvm::Value* is_proc_attach = cb_builder.CreateICmpEQ(reason, llvm::ConstantInt::get(i32_ty, 1));
        llvm::Value* is_thrd_attach = cb_builder.CreateICmpEQ(reason, llvm::ConstantInt::get(i32_ty, 2));
        llvm::Value* should_init = cb_builder.CreateOr(is_proc_attach, is_thrd_attach);
        cb_builder.CreateCondBr(should_init, cb_attach, cb_exit);

        cb_builder.SetInsertPoint(cb_attach);
        cb_builder.CreateCall(target_func);
        cb_builder.CreateBr(cb_exit);

        cb_builder.SetInsertPoint(cb_exit);
        cb_builder.CreateRet(llvm::ConstantInt::get(i32_ty, 0));

        llvm::GlobalVariable* cb_gv = nullptr;
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), ptr_ty, false, llvm::GlobalValue::ExternalLinkage, tls_callback, "__tls_cb", &cb_gv));
        cb_gv->setSection(".CRT$XDU");
        cb_gv->setVisibility(llvm::GlobalValue::HiddenVisibility);
      } else {
        llvm::FunctionType* atexit_ty = llvm::FunctionType::get(i32_ty, {ptr_ty, ptr_ty, ptr_ty}, false);
        llvm::Function* cxa_atexit = be_state->module->getFunction("__cxa_thread_atexit");
        if (!cxa_atexit) {
          cxa_atexit = llvm::Function::Create(atexit_ty, llvm::GlobalValue::ExternalLinkage, "__cxa_thread_atexit", be_state->module.get());
        }
      }

      be_state->builder->CreateRetVoid();
    }
    return llvm_gen_be_error_t::ok;
  }

  if (routine->overriding_function_for_wrapper) {
    return llvm_lower_thunk_body(routine);
  }
#endif /* DO_IL_LOWERING && IA64_ABI */

  return llvm_lower_function_body(routine);
}

/**
 * @brief Recursively lowers routine definitions within a scope, its namespaces, and its classes.
 * @details Traverses `scope->routines`, `scope->namespaces`, `scope->types` (for class/struct
 *          member routines), and `scope->scopes` to emit LLVM function bodies.
 * @param[in] scope The EDG scope to traverse.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t lower_routine_defs_in_scope(a_scope_ptr scope) noexcept {
  if (!be_state->visited_scopes.insert(scope).second) {
    return llvm_gen_be_error_t::ok;
  }
  if (be_state->dbg_state) {
    unsigned imp_cnt = 0;
    LLVM_GEN_BE_CHECK(emit_scope_imported_entities(be_state->dbg_state, scope, &imp_cnt));
  }
#if MAINTAIN_NEEDED_FLAGS || BACK_END_IS_GCC_GEN_BE
  bool is_primary = (scope == il_header.primary_scope);
#endif /* MAINTAIN_NEEDED_FLAGS || BACK_END_IS_GCC_GEN_BE */
  for (a_routine_ptr routine = scope->routines; routine != nullptr; routine = routine->next) {
    bool skip_rout = false;
    LLVM_GEN_BE_CHECK(is_unused_static_inline_routine(routine, &skip_rout));
    if (skip_rout) continue;
#if MAINTAIN_NEEDED_FLAGS || BACK_END_IS_GCC_GEN_BE
    if (!is_primary && il_header.function_def_table != nullptr &&
        !routine->definition_needed && !routine->source_corresp.referenced &&
        !routine->keep_definition_in_il) {
      continue;
    }
#endif /* MAINTAIN_NEEDED_FLAGS || BACK_END_IS_GCC_GEN_BE */
    llvm_gen_be_error_t err = lower_single_routine_definition(routine);
    if (err != llvm_gen_be_error_t::ok) return err;
    if (!ignore_routine_in_back_end(routine) &&
        routine->function_def_number != NULL_function_def_number) {
      a_scope_ptr fscope = nullptr;
      LLVM_GEN_BE_CHECK(get_scope_for_routine_definition(routine, &fscope));
      if (fscope) {
        LLVM_GEN_BE_CHECK(lower_routine_defs_in_scope(fscope));
      }
    }
  }
  for (a_namespace_ptr ns = scope->namespaces; ns != nullptr; ns = ns->next) {
    if (!ns->is_namespace_alias && ns->variant.assoc_scope) {
      llvm_gen_be_error_t err = lower_routine_defs_in_scope(ns->variant.assoc_scope);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  }
  for (a_type_ptr tp = scope->types; tp != nullptr; tp = tp->next) {
    a_scope_ptr cscope = nullptr;
    LLVM_GEN_BE_CHECK(get_valid_class_assoc_scope(tp, &cscope));
    if (cscope) {
      llvm_gen_be_error_t err = lower_routine_defs_in_scope(cscope);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  }
  for (a_scope_ptr sub = scope->scopes; sub != nullptr; sub = sub->next) {
    if (sub->kind != sck_func_prototype && !sub->is_expansion_prototype) {
      llvm_gen_be_error_t err = lower_routine_defs_in_scope(sub);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers all EDG routine definitions in the translation unit.
 * @details Traverses the primary file scope and all nested namespace and class scopes.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_lower_function_definitions(void) noexcept {
  be_state->visited_scopes.clear();
  be_state->demand_defined_routines.clear();
  if (!il_header.primary_scope) return llvm_gen_be_error_t::ok;
  llvm_gen_be_error_t err = lower_routine_defs_in_scope(il_header.primary_scope);
  if (err != llvm_gen_be_error_t::ok) return err;
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  for (a_scope_orphaned_list_header_ptr solhp = il_header.scope_orphaned_list_headers;
       solhp != nullptr; solhp = solhp->next) {
    if (solhp->in_expansion_prototype ||
        (solhp->assoc_routine && ignore_routine_in_back_end(solhp->assoc_routine))) continue;
    for (a_type_ptr tp = solhp->orphaned_types; tp != nullptr; tp = tp->next) {
      a_scope_ptr cscope = nullptr;
      LLVM_GEN_BE_CHECK(get_valid_class_assoc_scope(tp, &cscope));
      if (cscope) {
        LLVM_GEN_BE_CHECK(lower_routine_defs_in_scope(cscope));
      }
    }
  }
#endif
  for (size_t idx = 0; idx < be_state->demand_defined_routines.size(); ++idx) {
    a_routine_ptr r = be_state->demand_defined_routines[idx];
    if (!r || !r->source_corresp.name) continue;
    llvm::Function* existing_fn = be_state->module->getFunction(r->source_corresp.name);
    if (existing_fn && !existing_fn->empty()) continue;
    LLVM_GEN_BE_CHECK(llvm_lower_function_prototype(r));
    LLVM_GEN_BE_CHECK(lower_single_routine_definition(r));
  }
  be_state->demand_defined_routines.clear();
  for (llvm::Function& f : *be_state->module) {
    if (f.isDeclaration()) {
      f.setComdat(nullptr);
      if (f.hasLocalLinkage() || f.hasLinkOnceLinkage() || f.hasWeakLinkage()) {
        f.setLinkage(llvm::GlobalValue::ExternalLinkage);
      }
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers global constructors and destructors into llvm.global_ctors/dtors or MSVC .CRT sections.
 * @details Implements or declares `llvm_lower_global_ctors_and_dtors` for the EDG LLVM backend.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_lower_global_ctors_and_dtors(void) noexcept {
  std::vector<llvm::Constant*> ctors;
  std::vector<llvm::Constant*> dtors;
  std::vector<std::pair<int, llvm::Constant*>> msvc_ctors;
  std::vector<std::pair<int, llvm::Constant*>> msvc_dtors;
  
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::StructType* ctor_struct_ty = llvm::StructType::get(
      int32_ty, ptr_ty, ptr_ty); // { i32, void ()*, i8* }

  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  bool is_msvc = triple.getEnvironment() == llvm::Triple::MSVC;

  // Register any generated C++20 module initializer (_ZGIW<name> / __module_init_<name>) in global_ctors / .CRT$XCU
  if (trans_unit_module != nullptr) {
    for (llvm::Function& fn : *be_state->module) {
      if (!fn.isDeclaration() &&
          (fn.getName().starts_with("__module_init_") || fn.getName().starts_with("_ZGIW"))) {
        if (is_msvc) {
          msvc_ctors.push_back({200, &fn});
        } else {
          llvm::Constant* prio = llvm::ConstantInt::get(int32_ty, 200);
          llvm::Constant* null_p = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
          ctors.push_back(llvm::ConstantStruct::get(ctor_struct_ty, {prio, &fn, null_p}));
        }
      }
    }
  }

  bool has_prelowered_init_routine = false;
  if (il_header.primary_scope) {
    for (a_routine_ptr routine = il_header.primary_scope->routines; routine != nullptr; routine = routine->next) {
      if (!routine->source_corresp.name) continue;
      bool is_sti_ctor = (std::strncmp(routine->source_corresp.name, "__sti__", 7) == 0);
      bool is_std_dtor = (std::strncmp(routine->source_corresp.name, "__std__", 7) == 0);
      bool is_ctor = routine->is_initialization_routine || is_sti_ctor;
      bool is_dtor = routine->is_finalization_routine || is_std_dtor;
      if (is_ctor) {
        has_prelowered_init_routine = true;
      }
      int ctor_priority_val = 65535;
      int dtor_priority_val = 65535;

#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
      if (routine->has_ctor_priority) {
        is_ctor = true;
        ctor_priority_val = gnu_routine_supp(routine)->ctor_priority;
      }
      if (routine->has_dtor_priority) {
        is_dtor = true;
        dtor_priority_val = gnu_routine_supp(routine)->dtor_priority;
      }
#if DO_IL_LOWERING
      if (is_ctor && routine->init_priority != 0) {
         ctor_priority_val = routine->init_priority;
      }
      if (is_dtor && routine->init_priority != 0) {
         dtor_priority_val = routine->init_priority;
      }
#endif // DO_IL_LOWERING
#endif // GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
#endif // GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED

      if (is_ctor || is_dtor) {
        llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
        if (func) {
          llvm::Constant* fn_ptr = func;
          llvm::Constant* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
          
          if (is_ctor) {
            if (is_msvc) msvc_ctors.push_back({ctor_priority_val, fn_ptr});
            else {
              llvm::Constant* priority = llvm::ConstantInt::get(int32_ty, static_cast<uint64_t>(ctor_priority_val));
              ctors.push_back(llvm::ConstantStruct::get(ctor_struct_ty, {priority, fn_ptr, null_ptr}));
            }
          }
          if (is_dtor) {
            if (is_msvc) msvc_dtors.push_back({dtor_priority_val, fn_ptr});
            else {
              llvm::Constant* priority = llvm::ConstantInt::get(int32_ty, static_cast<uint64_t>(dtor_priority_val));
              dtors.push_back(llvm::ConstantStruct::get(ctor_struct_ty, {priority, fn_ptr, null_ptr}));
            }
          }
        }
      }
    }
  }

  if (!has_prelowered_init_routine && !be_state->global_dynamic_inits.empty()) {
    llvm::Type* void_ty = llvm::Type::getVoidTy(*be_state->context);
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
    llvm::FunctionType* void_fn_fty = llvm::FunctionType::get(void_ty, false);
    llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();

    for (const GlobalDynamicInitEntry& entry : be_state->global_dynamic_inits) {
      a_variable_ptr var = entry.var;
      a_dynamic_init_ptr dip = entry.dip;
      std::string vname;
      LLVM_GEN_BE_CHECK(llvm_get_mangled_variable_name(var, &vname));
      llvm::GlobalVariable* gvar = be_state->module->getNamedGlobal(vname);
      if (!gvar) continue;

      if (var->is_thread_local) {
        llvm::Function* tls_init_fn = be_state->module->getFunction("__tls_init");
        if (!tls_init_fn) {
          tls_init_fn = llvm::Function::Create(
              void_fn_fty, llvm::GlobalValue::InternalLinkage, "__tls_init", be_state->module.get());
        }
        LLVM_GEN_BE_CHECK(synthesize_tls_dynamic_init(tls_init_fn, is_msvc));
        continue;
      }

      std::string init_fn_name = "__cxx_global_var_init." + vname;
      llvm::Function* init_fn = be_state->module->getFunction(init_fn_name);
      if (!init_fn) {
        init_fn = llvm::Function::Create(
            void_fn_fty, llvm::GlobalValue::InternalLinkage, init_fn_name, be_state->module.get());
      }
      if (!init_fn->empty()) continue;

      llvm::Constant* assoc_comdat_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
      llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*be_state->context, "entry", init_fn);
      llvm::BasicBlock* do_init_bb = entry_bb;
      llvm::BasicBlock* done_bb = nullptr;

      if (var->is_inline) {
        std::string guard_name = (vname.rfind("_Z", 0) == 0)
                                     ? ("_ZGV" + vname.substr(2))
                                     : ("_ZGV" + std::to_string(vname.size()) + vname);
        llvm::GlobalVariable* guard_gv = be_state->module->getNamedGlobal(guard_name);
        if (!guard_gv) {
          LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), i64_ty, false, llvm::GlobalValue::LinkOnceODRLinkage, llvm::ConstantInt::get(i64_ty, 0), guard_name, &guard_gv));
        }
        if (llvm::Comdat* comdat = gvar->getComdat()) {
          guard_gv->setComdat(comdat);
          init_fn->setComdat(comdat);
          assoc_comdat_ptr = gvar;
        }
        do_init_bb = llvm::BasicBlock::Create(*be_state->context, "init.check", init_fn);
        done_bb = llvm::BasicBlock::Create(*be_state->context, "init.end", init_fn);
        be_state->builder->SetInsertPoint(entry_bb);
        llvm::Value* guard_byte = be_state->builder->CreateLoad(i8_ty, guard_gv, "guard.byte");
        llvm::Value* uninit = be_state->builder->CreateICmpEQ(guard_byte, llvm::ConstantInt::get(i8_ty, 0));
        be_state->builder->CreateCondBr(uninit, do_init_bb, done_bb);
        be_state->builder->SetInsertPoint(do_init_bb);
        be_state->builder->CreateStore(llvm::ConstantInt::get(i8_ty, 1), guard_gv);
      } else {
        be_state->builder->SetInsertPoint(entry_bb);
      }

      llvm_gen_be_error_t init_err = llvm_emit_dynamic_init(dip, gvar, gvar->getValueType());
      if (init_err != llvm_gen_be_error_t::ok) return init_err;

      if (dip->destructor) {
        const char* dtor_name = dip->destructor->source_corresp.name
                                    ? dip->destructor->source_corresp.name
                                    : "dtor";
        llvm::FunctionType* dtor_fty = llvm::FunctionType::get(void_ty, {ptr_ty}, false);
        llvm::FunctionCallee dtor_callee = be_state->module->getOrInsertFunction(dtor_name, dtor_fty);
        if (is_msvc) {
          std::string atexit_stub_name = "__dtor_" + vname;
          llvm::Function* atexit_stub = be_state->module->getFunction(atexit_stub_name);
          if (!atexit_stub) {
            atexit_stub = llvm::Function::Create(
                void_fn_fty, llvm::GlobalValue::InternalLinkage, atexit_stub_name, be_state->module.get());
            llvm::BasicBlock* stub_bb = llvm::BasicBlock::Create(*be_state->context, "entry", atexit_stub);
            llvm::IRBuilder<> stub_builder(stub_bb);
            stub_builder.CreateCall(dtor_callee, {gvar});
            stub_builder.CreateRetVoid();
          }
          llvm::FunctionType* msvc_atexit_ty = llvm::FunctionType::get(int32_ty, {ptr_ty}, false);
          llvm::FunctionCallee msvc_atexit = be_state->module->getOrInsertFunction("atexit", msvc_atexit_ty);
          be_state->builder->CreateCall(msvc_atexit, {atexit_stub});
        } else {
          llvm::FunctionType* cxa_atexit_ty = llvm::FunctionType::get(int32_ty, {ptr_ty, ptr_ty, ptr_ty}, false);
          llvm::FunctionCallee cxa_atexit = be_state->module->getOrInsertFunction("__cxa_atexit", cxa_atexit_ty);
          llvm::GlobalVariable* dso_handle = nullptr;
          LLVM_GEN_BE_CHECK(get_or_create_dso_handle(&dso_handle));
          be_state->builder->CreateCall(cxa_atexit, {dtor_callee.getCallee(), gvar, dso_handle});
        }
      }

      if (done_bb) {
        be_state->builder->CreateBr(done_bb);
        be_state->builder->SetInsertPoint(done_bb);
      }
      be_state->builder->CreateRetVoid();

      int priority_val = 65535;
#if GNU_EXTENSIONS_ALLOWED && GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
      if (var->init_priority != 0) {
        priority_val = var->init_priority;
      }
#endif
      if (is_msvc) {
        msvc_ctors.push_back({priority_val, init_fn});
      } else {
        llvm::Constant* priority = llvm::ConstantInt::get(int32_ty, static_cast<uint64_t>(priority_val));
        ctors.push_back(llvm::ConstantStruct::get(ctor_struct_ty, {priority, init_fn, assoc_comdat_ptr}));
      }
    }
    be_state->builder->restoreIP(saved_ip);
    be_state->global_dynamic_inits.clear();
  }

  if (is_msvc) {
      std::vector<llvm::Constant*> used_vars;
      // Group and emit MSVC ctors by section
      int ctor_idx = 0;
      for (const auto& ctor : msvc_ctors) {
          std::string section = ".CRT$XCU";
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
          if (ctor.first != 65535) section = ".CRT$XCT";
#endif
          llvm::GlobalVariable* crt_var = nullptr;
          LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), ptr_ty, false, llvm::GlobalValue::InternalLinkage, ctor.second, "msvc_ctor." + std::to_string(ctor_idx++), &crt_var));
          crt_var->setSection(section);
          used_vars.push_back(crt_var);
      }
      int dtor_idx = 0;
      for (const auto& dtor : msvc_dtors) {
          std::string section = ".CRT$XTX";
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
          if (dtor.first != 65535) section = ".CRT$XTY";
#endif
          llvm::GlobalVariable* crt_var = nullptr;
          LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), ptr_ty, false, llvm::GlobalValue::InternalLinkage, dtor.second, "msvc_dtor." + std::to_string(dtor_idx++), &crt_var));
          crt_var->setSection(section);
          used_vars.push_back(crt_var);
      }
      
      if (!used_vars.empty()) {
          llvm::ArrayType* used_array_ty = llvm::ArrayType::get(ptr_ty, used_vars.size());
          llvm::Constant* used_array = llvm::ConstantArray::get(used_array_ty, used_vars);
          llvm::GlobalVariable* used_gv = be_state->module->getGlobalVariable("llvm.used");
          if (!used_gv) {
              LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), used_array_ty, false, llvm::GlobalValue::AppendingLinkage, used_array, "llvm.used", &used_gv));
              used_gv->setSection("llvm.metadata");
          } else {
              llvm::ConstantArray* old_init = llvm::cast<llvm::ConstantArray>(used_gv->getInitializer());
              std::vector<llvm::Constant*> new_elements;
              for (unsigned i = 0; i < old_init->getNumOperands(); ++i) {
                  new_elements.push_back(llvm::cast<llvm::Constant>(old_init->getOperand(i)));
              }
              for (llvm::Constant* c : used_vars) {
                  new_elements.push_back(c);
              }
              used_gv->eraseFromParent();
              llvm::ArrayType* new_used_array_ty = llvm::ArrayType::get(ptr_ty, new_elements.size());
              llvm::Constant* new_used_array = llvm::ConstantArray::get(new_used_array_ty, new_elements);
              LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), new_used_array_ty, false, llvm::GlobalValue::AppendingLinkage, new_used_array, "llvm.used", &used_gv));
              used_gv->setSection("llvm.metadata");
          }
      }
  } else {
      if (!ctors.empty()) {
        llvm::ArrayType* ctors_array_ty = llvm::ArrayType::get(ctor_struct_ty, ctors.size());
        llvm::Constant* ctors_array = llvm::ConstantArray::get(ctors_array_ty, ctors);
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), ctors_array_ty, false, llvm::GlobalValue::AppendingLinkage, ctors_array, "llvm.global_ctors"));
      }
      
      if (!dtors.empty()) {
        llvm::ArrayType* dtors_array_ty = llvm::ArrayType::get(ctor_struct_ty, dtors.size());
        llvm::Constant* dtors_array = llvm::ConstantArray::get(dtors_array_ty, dtors);
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), dtors_array_ty, false, llvm::GlobalValue::AppendingLinkage, dtors_array, "llvm.global_dtors"));
      }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Sanitizes a C++20 module or header-unit name into a valid C identifier suffix.
 * @details Replaces non-alphanumeric characters (such as `:`, `.`, `/`, `-`) with `_`.
 * @param[in,out] io_name Pointer to the module name string to sanitize in-place.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t sanitize_module_identifier(std::string* io_name) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  for (char& ch : *io_name) {
    if (!std::isalnum(static_cast<unsigned char>(ch))) {
      ch = '_';
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers C++20 Module interfaces, partitions, and global linkage visibility settings.
 * @details Emits module flags (`C++20 Module`, `C++20 Module Partition`), named metadata nodes
 *          (`!edg.cpp20.module`, `!edg.cpp20.imports`), a module initialization guard variable
 *          (`__module_init_guard_<name>`), and an idempotent module initializer routine
 *          (`__module_init_<name>`) that invokes imported module initializers and local dynamic
 *          initialization routines.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_module_metadata(void) noexcept {
  a_module_ptr curr_tu_mod = trans_unit_module;
  llvm::LLVMContext& Ctx = *be_state->context;
  llvm::Module* M = be_state->module.get();

  std::vector<std::string> imported_mod_names;
  llvm::NamedMDNode* imports_md = nullptr;
  for (a_module_import_decl_ptr midp = il_header.imported_modules; midp != nullptr; midp = midp->next) {
    if (midp->module_info) {
      const char* imp_name = nullptr;
      if (midp->module_info->kind == mk_unit) {
        imp_name = midp->module_info->variant.unit.name;
      } else if (midp->module_info->kind == mk_unit_partition) {
        imp_name = midp->module_info->variant.unit_partition.name;
      } else if (midp->module_info->kind == mk_header_unit) {
        imp_name = midp->module_info->variant.header_unit.name;
      }
      if (imp_name && imp_name[0] != '\0') {
        imported_mod_names.push_back(imp_name);
        if (!imports_md) {
          imports_md = M->getOrInsertNamedMetadata("edg.cpp20.imports");
        }
        const char* bmi_path = midp->module_info->resolved_file ? midp->module_info->resolved_file : "";
        llvm::MDNode* imp_node = llvm::MDNode::get(Ctx, {
            llvm::MDString::get(Ctx, imp_name),
            llvm::MDString::get(Ctx, bmi_path)
        });
        imports_md->addOperand(imp_node);
      }
    }
  }

  if (curr_tu_mod != nullptr) {
    const char* mod_cstr = nullptr;
    if (curr_tu_mod->kind == mk_unit) {
      mod_cstr = curr_tu_mod->variant.unit.name;
    } else if (curr_tu_mod->kind == mk_unit_partition) {
      mod_cstr = curr_tu_mod->variant.unit_partition.name;
    }
    std::string module_name = mod_cstr ? mod_cstr : "";

    if (!module_name.empty()) {
      M->addModuleFlag(llvm::Module::Warning, "C++20 Module", 1);
      if (curr_tu_mod->kind == mk_unit_partition) {
        M->addModuleFlag(llvm::Module::Warning, "C++20 Module Partition", 1);
      }

      // Collect exported symbols in primary scope for BMI metadata serialization
      std::vector<llvm::Metadata*> exported_syms;
      if (il_header.primary_scope) {
        for (a_routine_ptr r = il_header.primary_scope->routines; r != nullptr; r = r->next) {
          if (r->source_corresp.name && r->storage_class != sc_static &&
              r->source_corresp.name_linkage != nlk_internal) {
            exported_syms.push_back(llvm::MDString::get(Ctx, r->source_corresp.name));
          }
        }
        for (a_variable_ptr v = il_header.primary_scope->variables; v != nullptr; v = v->next) {
          if (v->source_corresp.name && v->storage_class != sc_static &&
              v->source_corresp.name_linkage != nlk_internal) {
            exported_syms.push_back(llvm::MDString::get(Ctx, v->source_corresp.name));
          }
        }
      }

      const char* bmi_file = curr_tu_mod->resolved_file ? curr_tu_mod->resolved_file : "";
      llvm::NamedMDNode* mod_md = M->getOrInsertNamedMetadata("edg.cpp20.module");
      llvm::MDNode* exported_tuple = llvm::MDTuple::get(Ctx, exported_syms);
      mod_md->addOperand(llvm::MDNode::get(Ctx, {
          llvm::MDString::get(Ctx, module_name),
          llvm::MDString::get(Ctx, bmi_file),
          exported_tuple
      }));

      // Sanitize module name for symbol identifiers
      std::string sanitized_name = module_name;
      LLVM_GEN_BE_CHECK(sanitize_module_identifier(&sanitized_name));

      std::string bmi_summary_name = "__edg_bmi_summary_" + sanitized_name;
      if (!M->getGlobalVariable(bmi_summary_name, true)) {
        llvm::Constant* bmi_str = llvm::ConstantDataArray::getString(Ctx, module_name, true);
        llvm::GlobalVariable* bmi_gv = nullptr;
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(M, bmi_str->getType(), true, llvm::GlobalValue::InternalLinkage, bmi_str, bmi_summary_name, &bmi_gv));
        bmi_gv->setSection(llvm::Triple(M->getTargetTriple()).isOSBinFormatMachO()
                               ? "__DATA,__edg_bmi"
                               : ".edg_bmi");
      }

      std::string guard_name = "__module_init_guard_" + sanitized_name;
      std::string init_fn_name = "__module_init_" + sanitized_name;
      std::string itanium_init_name = "_ZGIW" + std::to_string(sanitized_name.size()) + sanitized_name;
      std::string itanium_guard_name = "_ZGVW" + std::to_string(sanitized_name.size()) + sanitized_name;
      llvm::Type* i8_ty = llvm::Type::getInt8Ty(Ctx);
      llvm::GlobalVariable* guard_gv = M->getGlobalVariable(guard_name, true);
      if (!guard_gv) {
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(M, i8_ty, false, llvm::GlobalValue::InternalLinkage, llvm::ConstantInt::get(i8_ty, 0), guard_name, &guard_gv));
      }
      if (!M->getGlobalVariable(itanium_guard_name, true)) {
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(M, i8_ty, false, llvm::GlobalValue::InternalLinkage, llvm::ConstantInt::get(i8_ty, 0), itanium_guard_name));
      }
      llvm::FunctionType* void_fn_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), false);
      llvm::Function* mod_init_fn = M->getFunction(init_fn_name);
      if (!mod_init_fn) {
        mod_init_fn = llvm::Function::Create(
            void_fn_ty, llvm::GlobalValue::ExternalLinkage, init_fn_name, M);
      }
      if (mod_init_fn->empty()) {
        llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(Ctx, "entry", mod_init_fn);
        llvm::BasicBlock* do_init_bb = llvm::BasicBlock::Create(Ctx, "init", mod_init_fn);
        llvm::BasicBlock* done_bb = llvm::BasicBlock::Create(Ctx, "done", mod_init_fn);

        llvm::IRBuilder<> init_builder(entry_bb);
        llvm::Value* guard_val = init_builder.CreateLoad(i8_ty, guard_gv, "guard.val");
        llvm::Value* already_init = init_builder.CreateICmpNE(guard_val, llvm::ConstantInt::get(i8_ty, 0));
        init_builder.CreateCondBr(already_init, done_bb, do_init_bb);

        init_builder.SetInsertPoint(do_init_bb);
        init_builder.CreateStore(llvm::ConstantInt::get(i8_ty, 1), guard_gv);

        for (const std::string& imp : imported_mod_names) {
          std::string imp_sanitized = imp;
          LLVM_GEN_BE_CHECK(sanitize_module_identifier(&imp_sanitized));
          llvm::FunctionCallee imp_init = M->getOrInsertFunction("__module_init_" + imp_sanitized, void_fn_ty);
          init_builder.CreateCall(imp_init);
          std::string imp_itanium = "_ZGIW" + std::to_string(imp_sanitized.size()) + imp_sanitized;
          llvm::FunctionCallee imp_itanium_init = M->getOrInsertFunction(imp_itanium, void_fn_ty);
          init_builder.CreateCall(imp_itanium_init);
        }

        if (il_header.primary_scope) {
          for (a_routine_ptr r = il_header.primary_scope->routines; r != nullptr; r = r->next) {
            if (r->is_initialization_routine && r->source_corresp.name) {
              llvm::Function* r_fn = M->getFunction(r->source_corresp.name);
              if (r_fn && r_fn != mod_init_fn && r_fn->arg_size() == 0) {
                init_builder.CreateCall(r_fn);
              }
            }
          }
        }
        init_builder.CreateBr(done_bb);

        init_builder.SetInsertPoint(done_bb);
        init_builder.CreateRetVoid();

        llvm::Function* itanium_init_fn = llvm::Function::Create(
            void_fn_ty, llvm::GlobalValue::ExternalLinkage, itanium_init_name, M);
        llvm::BasicBlock* it_entry_bb = llvm::BasicBlock::Create(Ctx, "entry", itanium_init_fn);
        llvm::IRBuilder<> it_builder(it_entry_bb);
        it_builder.CreateCall(mod_init_fn);
        it_builder.CreateRetVoid();
      }
    }
  }

  return llvm_gen_be_error_t::ok;
}
END_EDG_NAMESPACE
#endif
