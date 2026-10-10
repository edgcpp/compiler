/**
 * @file gcc_gen_be_type.h
 * @brief Type lowering for the GCC backend.
 *
 * This file declares the function responsible for lowering an EDG frontend
 * type (`a_type_ptr`) into a libgccjit type (`gcc_jit_type`).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_TYPE_H
#define GCC_GEN_BE_TYPE_H

#include "gcc_gen_be_error.h"
#include "fe_common.h"
#include "types.h"

struct gcc_jit_type;

BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers an EDG frontend type into a libgccjit type.
 *
 * @param tp The frontend type to lower.
 * @param out_type A pointer to a gcc_jit_type pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_type` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_TYPE_H */