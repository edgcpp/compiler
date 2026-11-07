/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

ifc_map_functions_dbg.c -- Function implementations for IFC-based module
                           debug functions.

** NOTICE: This file is produced by an external script. **

While EDG staff should update the generation script rather than manually
editing this file, customers are welcome to modify this file and create patches
as they see fit.

Please contact EDG Support if you would be interested in using, or learning
more about, the tool that generated this file.
*/

#include "basic_hdrs.h"
#include "checking.h"
#include "header_util.h"
#include "ifc_map.h"
#include "ifc_map_functions.h"
#include "ifc_modules_internal.h"


#if !STANDALONE_UTILITY_PROGRAM

#if DEBUG

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/* Disable spurious GCC warnings in generated code. */
BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED


void db_node(const an_ifc_keyword_syntax &universal, unsigned indent)
/*
Given the universal representation of KeywordSyntax, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_value(universal)) {
    an_ifc_keyword_sort field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value: %s\n", str_for(field));
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_keyword_syntax &universal)
/*
Given the universal representation of KeywordSyntax, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "KeywordSyntax ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_module_reference &universal, unsigned indent)
/*
Given the universal representation of ModuleReference, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_owner(universal)) {
    an_ifc_text_offset field = get_ifc_owner(universal);

    db_print_indent(indent);
    fprintf(f_debug, "owner: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_partition(universal)) {
    an_ifc_text_offset field = get_ifc_partition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "partition: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_module_reference &universal)
/*
Given the universal representation of ModuleReference, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "ModuleReference ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_nestable_word &universal, unsigned indent)
/*
Given the universal representation of NestableWord, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_category(universal)) {
    an_ifc_word_category field = get_ifc_category(universal);

    db_print_indent(indent);
    fprintf(f_debug, "category:\n");
    db_print_indent(indent);
    fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
    ++indent;
    switch (field.sort) {
      case ifc_ws_source_directive:
        { an_ifc_source_directive_sort &sd_ref =
                                                field.variant.source_directive;

          db_print_indent(indent);
          fprintf(f_debug, "source_directive: %s\n", str_for(sd_ref));
        }
        break;
      case ifc_ws_source_identifier:
        { an_ifc_source_identifier_category &si_ref =
                                               field.variant.source_identifier;

          db_print_indent(indent);
          fprintf(f_debug, "source_identifier:\n");
          db_print_indent(indent);
          fprintf(f_debug, "  sort: %s\n", str_for(si_ref.sort));
          ++indent;
          switch (si_ref.sort) {
            case ifc_sis_msvc:
              { an_ifc_source_unknown_identifier &m_ref = si_ref.variant.msvc;

                db_print_indent(indent);
                fprintf(f_debug, "msvc: %llu\n",
                        (unsigned long long)m_ref.value);
              }
              break;
            case ifc_sis_msvc_builtin_huge_val:
              { an_ifc_source_unknown_identifier &mbhv_ref =
                                          si_ref.variant.msvc_builtin_huge_val;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_builtin_huge_val: %llu\n",
                        (unsigned long long)mbhv_ref.value);
              }
              break;
            case ifc_sis_msvc_builtin_huge_valf:
              { an_ifc_source_unknown_identifier &mbhv_ref =
                                         si_ref.variant.msvc_builtin_huge_valf;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_builtin_huge_valf: %llu\n",
                        (unsigned long long)mbhv_ref.value);
              }
              break;
            case ifc_sis_msvc_builtin_nan:
              { an_ifc_source_unknown_identifier &mbn_ref =
                                               si_ref.variant.msvc_builtin_nan;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_builtin_nan: %llu\n",
                        (unsigned long long)mbn_ref.value);
              }
              break;
            case ifc_sis_msvc_builtin_nanf:
              { an_ifc_source_unknown_identifier &mbn_ref =
                                              si_ref.variant.msvc_builtin_nanf;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_builtin_nanf: %llu\n",
                        (unsigned long long)mbn_ref.value);
              }
              break;
            case ifc_sis_msvc_builtin_nans:
              { an_ifc_source_unknown_identifier &mbn_ref =
                                              si_ref.variant.msvc_builtin_nans;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_builtin_nans: %llu\n",
                        (unsigned long long)mbn_ref.value);
              }
              break;
            case ifc_sis_msvc_builtin_nansf:
              { an_ifc_source_unknown_identifier &mbn_ref =
                                             si_ref.variant.msvc_builtin_nansf;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_builtin_nansf: %llu\n",
                        (unsigned long long)mbn_ref.value);
              }
              break;
            case ifc_sis_plain:
              { an_ifc_text_offset &p_ref = si_ref.variant.plain;

                db_print_indent(indent);
                fprintf(f_debug, "plain: %llu\n",
                        (unsigned long long)p_ref.value);
              }
              break;
            default_is_unexpected();
          }  /* switch */
          --indent;
        }
        break;
      case ifc_ws_source_keyword:
        { an_ifc_source_keyword_sort &sk_ref = field.variant.source_keyword;

          db_print_indent(indent);
          fprintf(f_debug, "source_keyword: %s\n", str_for(sk_ref));
        }
        break;
      case ifc_ws_source_literal:
        { an_ifc_source_literal_category &sl_ref =
                                                  field.variant.source_literal;

          db_print_indent(indent);
          fprintf(f_debug, "source_literal:\n");
          db_print_indent(indent);
          fprintf(f_debug, "  sort: %s\n", str_for(sl_ref.sort));
          ++indent;
          switch (sl_ref.sort) {
            case ifc_sls_defined_string:
              { an_ifc_string_index &ds_ref = sl_ref.variant.defined_string;

                db_print_indent(indent);
                fprintf(f_debug, "defined_string:");
                if (is_null_index(ds_ref)) {
                  fprintf(f_debug, " NULL\n");
                } else {
                  fprintf(f_debug, "\n");
                  db_print_indent(indent);
                  fprintf(f_debug, "  sort: %s\n", str_for(ds_ref.sort));
                  db_print_indent(indent);
                  fprintf(f_debug, "  value: %llu\n",
                          (unsigned long long)ds_ref.value);
                }  /* if */
              }
              break;
            case ifc_sls_msvc:
              { an_ifc_source_unknown_literal &m_ref = sl_ref.variant.msvc;

                db_print_indent(indent);
                fprintf(f_debug, "msvc: %llu\n",
                        (unsigned long long)m_ref.value);
              }
              break;
            case ifc_sls_msvc_binding:
              { an_ifc_expr_index &mb_ref = sl_ref.variant.msvc_binding;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_binding:");
                if (is_null_index(mb_ref)) {
                  fprintf(f_debug, " NULL\n");
                } else {
                  fprintf(f_debug, "\n");
                  db_print_indent(indent);
                  fprintf(f_debug, "  sort: %s\n", str_for(mb_ref.sort));
                  db_print_indent(indent);
                  fprintf(f_debug, "  value: %llu\n",
                          (unsigned long long)mb_ref.value);
                }  /* if */
              }
              break;
            case ifc_sls_msvc_cast_target_type:
              { an_ifc_type_index &mctt_ref =
                                          sl_ref.variant.msvc_cast_target_type;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_cast_target_type:");
                if (is_null_index(mctt_ref)) {
                  fprintf(f_debug, " NULL\n");
                } else {
                  fprintf(f_debug, "\n");
                  db_print_indent(indent);
                  fprintf(f_debug, "  sort: %s\n", str_for(mctt_ref.sort));
                  db_print_indent(indent);
                  fprintf(f_debug, "  value: %llu\n",
                          (unsigned long long)mctt_ref.value);
                }  /* if */
              }
              break;
            case ifc_sls_msvc_defined_constant:
              { an_ifc_expr_index &mdc_ref =
                                          sl_ref.variant.msvc_defined_constant;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_defined_constant:");
                if (is_null_index(mdc_ref)) {
                  fprintf(f_debug, " NULL\n");
                } else {
                  fprintf(f_debug, "\n");
                  db_print_indent(indent);
                  fprintf(f_debug, "  sort: %s\n", str_for(mdc_ref.sort));
                  db_print_indent(indent);
                  fprintf(f_debug, "  value: %llu\n",
                          (unsigned long long)mdc_ref.value);
                }  /* if */
              }
              break;
            case ifc_sls_msvc_function_name_macro:
              { an_ifc_text_offset &mfnm_ref =
                                       sl_ref.variant.msvc_function_name_macro;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_function_name_macro: %llu\n",
                        (unsigned long long)mfnm_ref.value);
              }
              break;
            case ifc_sls_msvc_resolved_type:
              { an_ifc_type_index &mrt_ref = sl_ref.variant.msvc_resolved_type;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_resolved_type:");
                if (is_null_index(mrt_ref)) {
                  fprintf(f_debug, " NULL\n");
                } else {
                  fprintf(f_debug, "\n");
                  db_print_indent(indent);
                  fprintf(f_debug, "  sort: %s\n", str_for(mrt_ref.sort));
                  db_print_indent(indent);
                  fprintf(f_debug, "  value: %llu\n",
                          (unsigned long long)mrt_ref.value);
                }  /* if */
              }
              break;
            case ifc_sls_msvc_string_prefix_macro:
              { an_ifc_text_offset &mspm_ref =
                                       sl_ref.variant.msvc_string_prefix_macro;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_string_prefix_macro: %llu\n",
                        (unsigned long long)mspm_ref.value);
              }
              break;
            case ifc_sls_scalar:
              { an_ifc_expr_index &s_ref = sl_ref.variant.scalar;

                db_print_indent(indent);
                fprintf(f_debug, "scalar:");
                if (is_null_index(s_ref)) {
                  fprintf(f_debug, " NULL\n");
                } else {
                  fprintf(f_debug, "\n");
                  db_print_indent(indent);
                  fprintf(f_debug, "  sort: %s\n", str_for(s_ref.sort));
                  db_print_indent(indent);
                  fprintf(f_debug, "  value: %llu\n",
                          (unsigned long long)s_ref.value);
                }  /* if */
              }
              break;
            case ifc_sls_string:
              { an_ifc_string_index &s_ref = sl_ref.variant.string;

                db_print_indent(indent);
                fprintf(f_debug, "string:");
                if (is_null_index(s_ref)) {
                  fprintf(f_debug, " NULL\n");
                } else {
                  fprintf(f_debug, "\n");
                  db_print_indent(indent);
                  fprintf(f_debug, "  sort: %s\n", str_for(s_ref.sort));
                  db_print_indent(indent);
                  fprintf(f_debug, "  value: %llu\n",
                          (unsigned long long)s_ref.value);
                }  /* if */
              }
              break;
            case ifc_sls_unknown:
              { an_ifc_source_unknown_literal &u_ref = sl_ref.variant.unknown;

                db_print_indent(indent);
                fprintf(f_debug, "unknown: %llu\n",
                        (unsigned long long)u_ref.value);
              }
              break;
            default_is_unexpected();
          }  /* switch */
          --indent;
        }
        break;
      case ifc_ws_source_operator:
        { an_ifc_source_operator_sort &so_ref = field.variant.source_operator;

          db_print_indent(indent);
          fprintf(f_debug, "source_operator: %s\n", str_for(so_ref));
        }
        break;
      case ifc_ws_source_punctuator:
        { an_ifc_source_punctuator_sort &sp_ref =
                                               field.variant.source_punctuator;

          db_print_indent(indent);
          fprintf(f_debug, "source_punctuator: %s\n", str_for(sp_ref));
        }
        break;
      case ifc_ws_unknown:
        { an_ifc_source_unknown_word &u_ref = field.variant.unknown;

          db_print_indent(indent);
          fprintf(f_debug, "unknown: %llu\n",
                  (unsigned long long)u_ref.value);
        }
        break;
      default_is_unexpected();
    }  /* switch */
    --indent;
  }  /* if */
  if (has_ifc_index(universal)) {
    an_ifc_index field = get_ifc_index(universal);

    db_print_indent(indent);
    fprintf(f_debug, "index: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_sort(universal)) {
    an_ifc_word_sort field = get_ifc_sort(universal);

    db_print_indent(indent);
    fprintf(f_debug, "sort: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_value(universal)) {
    an_ifc_u16 field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_nestable_word &universal)
/*
Given the universal representation of NestableWord, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "NestableWord ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_noexcept_specification &universal, unsigned indent)
/*
Given the universal representation of NoexceptSpecification, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_expr(universal)) {
    an_ifc_expr_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_sort(universal)) {
    an_ifc_noexcept_sort field = get_ifc_sort(universal);

    db_print_indent(indent);
    fprintf(f_debug, "sort: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_words(universal)) {
    an_ifc_sentence_index field = get_ifc_words(universal);

    db_print_indent(indent);
    fprintf(f_debug, "words: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_noexcept_specification &universal)
/*
Given the universal representation of NoexceptSpecification, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "NoexceptSpecification ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_parameterized_entity &universal, unsigned indent)
/*
Given the universal representation of ParameterizedEntity, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_attributes(universal)) {
    an_ifc_sentence_index field = get_ifc_attributes(universal);

    db_print_indent(indent);
    fprintf(f_debug, "attributes: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_body(universal)) {
    an_ifc_sentence_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_head(universal)) {
    an_ifc_sentence_index field = get_ifc_head(universal);

    db_print_indent(indent);
    fprintf(f_debug, "head: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_parameterized_entity &universal)
/*
Given the universal representation of ParameterizedEntity, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "ParameterizedEntity ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_sequence &universal, unsigned indent)
/*
Given the universal representation of Sequence, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_sequence &universal)
/*
Given the universal representation of Sequence, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "Sequence ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_source_location &universal, unsigned indent)
/*
Given the universal representation of SourceLocation, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_column(universal)) {
    an_ifc_column field = get_ifc_column(universal);

    db_print_indent(indent);
    fprintf(f_debug, "column: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_line(universal)) {
    an_ifc_line_offset field = get_ifc_line(universal);

    db_print_indent(indent);
    fprintf(f_debug, "line: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_source_location &universal)
/*
Given the universal representation of SourceLocation, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "SourceLocation ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_placeholder_basis_wrapper &universal,
             unsigned                                    indent)
/*
Given the universal representation of TypePlaceholderBasisWrapper, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_type_basis_sort field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value: %s\n", str_for(field));
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_placeholder_basis_wrapper &universal)
/*
Given the universal representation of TypePlaceholderBasisWrapper, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================= ");
  fprintf(f_debug, "TypePlaceholderBasisWrapper ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_file_header &universal, unsigned indent)
/*
Given the universal representation of FileHeader, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_abi(universal)) {
    an_ifc_abi field = get_ifc_abi(universal);

    db_print_indent(indent);
    fprintf(f_debug, "abi: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_arch(universal)) {
    an_ifc_architecture_sort field = get_ifc_arch(universal);

    db_print_indent(indent);
    fprintf(f_debug, "arch: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_checksum(universal)) {
    db_print_indent(indent);
    fprintf(f_debug, "checksum: UNIMPLEMENTED\n");
  }  /* if */
  if (has_ifc_dialect(universal)) {
    an_ifc_language_version field = get_ifc_dialect(universal);

    db_print_indent(indent);
    fprintf(f_debug, "dialect: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_global_scope(universal)) {
    an_ifc_scope_offset field = get_ifc_global_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "global_scope: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_internal(universal)) {
    an_ifc_bool field = get_ifc_internal(universal);

    db_print_indent(indent);
    fprintf(f_debug, "internal: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_major_version(universal)) {
    an_ifc_version field = get_ifc_major_version(universal);

    db_print_indent(indent);
    fprintf(f_debug, "major_version: %llu\n",
            (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_minor_version(universal)) {
    an_ifc_version field = get_ifc_minor_version(universal);

    db_print_indent(indent);
    fprintf(f_debug, "minor_version: %llu\n",
            (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_partition_count(universal)) {
    an_ifc_cardinality field = get_ifc_partition_count(universal);

    db_print_indent(indent);
    fprintf(f_debug, "partition_count: %llu\n",
            (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_src_path(universal)) {
    an_ifc_text_offset field = get_ifc_src_path(universal);

    db_print_indent(indent);
    fprintf(f_debug, "src_path: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_string_table_bytes(universal)) {
    an_ifc_byte_offset field = get_ifc_string_table_bytes(universal);

    db_print_indent(indent);
    fprintf(f_debug, "string_table_bytes: %llu\n",
            (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_string_table_size(universal)) {
    an_ifc_cardinality field = get_ifc_string_table_size(universal);

    db_print_indent(indent);
    fprintf(f_debug, "string_table_size: %llu\n",
            (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_toc(universal)) {
    an_ifc_byte_offset field = get_ifc_toc(universal);

    db_print_indent(indent);
    fprintf(f_debug, "toc: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_unit(universal)) {
    an_ifc_unit_index field = get_ifc_unit(universal);

    db_print_indent(indent);
    fprintf(f_debug, "unit:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_file_header &universal)
/*
Given the universal representation of FileHeader, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "FileHeader ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_partition &universal, unsigned indent)
/*
Given the universal representation of Partition, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_entry_size(universal)) {
    an_ifc_entity_size field = get_ifc_entry_size(universal);

    db_print_indent(indent);
    fprintf(f_debug, "entry_size: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_offset(universal)) {
    an_ifc_byte_offset field = get_ifc_offset(universal);

    db_print_indent(indent);
    fprintf(f_debug, "offset: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_partition &universal)
/*
Given the universal representation of Partition, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "Partition ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_attr_basic &universal, unsigned indent)
/*
Given the universal representation of AttrBasic, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_word(universal)) {
    an_ifc_nestable_word field = get_ifc_word(universal);

    db_print_indent(indent);
    fprintf(f_debug, "word:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_attr_basic &universal)
/*
Given the universal representation of AttrBasic, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "AttrBasic ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_attr_called &universal, unsigned indent)
/*
Given the universal representation of AttrCalled, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_arguments(universal)) {
    an_ifc_attr_index field = get_ifc_arguments(universal);

    db_print_indent(indent);
    fprintf(f_debug, "arguments:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_function(universal)) {
    an_ifc_attr_index field = get_ifc_function(universal);

    db_print_indent(indent);
    fprintf(f_debug, "function:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_attr_called &universal)
/*
Given the universal representation of AttrCalled, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "AttrCalled ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_attr_elaborated &universal, unsigned indent)
/*
Given the universal representation of AttrElaborated, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_expression(universal)) {
    an_ifc_expr_index field = get_ifc_expression(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expression:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_attr_elaborated &universal)
/*
Given the universal representation of AttrElaborated, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "AttrElaborated ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_attr_expanded &universal, unsigned indent)
/*
Given the universal representation of AttrExpanded, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_operand(universal)) {
    an_ifc_attr_index field = get_ifc_operand(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operand:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_attr_expanded &universal)
/*
Given the universal representation of AttrExpanded, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "AttrExpanded ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_attr_factored &universal, unsigned indent)
/*
Given the universal representation of AttrFactored, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_factor(universal)) {
    an_ifc_nestable_word field = get_ifc_factor(universal);

    db_print_indent(indent);
    fprintf(f_debug, "factor:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_terms(universal)) {
    an_ifc_attr_index field = get_ifc_terms(universal);

    db_print_indent(indent);
    fprintf(f_debug, "terms:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_attr_factored &universal)
/*
Given the universal representation of AttrFactored, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "AttrFactored ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_attr_labeled &universal, unsigned indent)
/*
Given the universal representation of AttrLabeled, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_attribute(universal)) {
    an_ifc_attr_index field = get_ifc_attribute(universal);

    db_print_indent(indent);
    fprintf(f_debug, "attribute:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_label(universal)) {
    an_ifc_nestable_word field = get_ifc_label(universal);

    db_print_indent(indent);
    fprintf(f_debug, "label:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_attr_labeled &universal)
/*
Given the universal representation of AttrLabeled, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "AttrLabeled ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_attr_scoped &universal, unsigned indent)
/*
Given the universal representation of AttrScoped, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_member(universal)) {
    an_ifc_nestable_word field = get_ifc_member(universal);

    db_print_indent(indent);
    fprintf(f_debug, "member:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_scope(universal)) {
    an_ifc_nestable_word field = get_ifc_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "scope:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_attr_scoped &universal)
/*
Given the universal representation of AttrScoped, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "AttrScoped ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_attr_tuple &universal, unsigned indent)
/*
Given the universal representation of AttrTuple, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_attr_tuple &universal)
/*
Given the universal representation of AttrTuple, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "AttrTuple ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_chart_multilevel &universal, unsigned indent)
/*
Given the universal representation of ChartMultilevel, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_chart_multilevel &universal)
/*
Given the universal representation of ChartMultilevel, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "ChartMultilevel ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_chart_unilevel &universal, unsigned indent)
/*
Given the universal representation of ChartUnilevel, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_constraint(universal)) {
    an_ifc_expr_index field = get_ifc_constraint(universal);

    db_print_indent(indent);
    fprintf(f_debug, "constraint:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_chart_unilevel &universal)
/*
Given the universal representation of ChartUnilevel, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "ChartUnilevel ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_const_f64 &universal, unsigned indent)
/*
Given the universal representation of ConstF64, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    db_print_indent(indent);
    fprintf(f_debug, "value: UNIMPLEMENTED\n");
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_const_f64 &universal)
/*
Given the universal representation of ConstF64, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "ConstF64 ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_const_i64 &universal, unsigned indent)
/*
Given the universal representation of ConstI64, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_u64 field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_const_i64 &universal)
/*
Given the universal representation of ConstI64, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "ConstI64 ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_const_str &universal, unsigned indent)
/*
Given the universal representation of ConstStr, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_length(universal)) {
    an_ifc_cardinality field = get_ifc_length(universal);

    db_print_indent(indent);
    fprintf(f_debug, "length: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_text_offset field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_suffix(universal)) {
    an_ifc_text_offset field = get_ifc_suffix(universal);

    db_print_indent(indent);
    fprintf(f_debug, "suffix: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_const_str &universal)
/*
Given the universal representation of ConstStr, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "ConstStr ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_alias &universal, unsigned indent)
/*
Given the universal representation of DeclAlias, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_aliasee(universal)) {
    an_ifc_type_index field = get_ifc_aliasee(universal);

    db_print_indent(indent);
    fprintf(f_debug, "aliasee:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_alias &universal)
/*
Given the universal representation of DeclAlias, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "DeclAlias ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_barren &universal, unsigned indent)
/*
Given the universal representation of DeclBarren, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_directive(universal)) {
    an_ifc_dir_index field = get_ifc_directive(universal);

    db_print_indent(indent);
    fprintf(f_debug, "directive:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_barren &universal)
/*
Given the universal representation of DeclBarren, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "DeclBarren ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_bitfield &universal, unsigned indent)
/*
Given the universal representation of DeclBitfield, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_properties(universal)) {
    an_ifc_reachable_properties_bitfield field = get_ifc_properties(universal);

    fprintf(f_debug, "properties:\n");
    ++indent;
    if (test_bitmask<ifc_rpb_all>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- All\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_attributes>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Attributes\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_default_arguments>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DefaultArguments\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_initializer>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Initializer\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_traits(universal)) {
    an_ifc_object_traits_bitfield field = get_ifc_traits(universal);

    fprintf(f_debug, "traits:\n");
    ++indent;
    if (test_bitmask<ifc_otb_constexpr>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Constexpr\n");
    }  /* if */
    if (test_bitmask<ifc_otb_initializer_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializerExported\n");
    }  /* if */
    if (test_bitmask<ifc_otb_inline>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Inline\n");
    }  /* if */
    if (test_bitmask<ifc_otb_mutable>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Mutable\n");
    }  /* if */
    if (test_bitmask<ifc_otb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_otb_thread_local>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- ThreadLocal\n");
    }  /* if */
    if (test_bitmask<ifc_otb_vendor>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vendor\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_width(universal)) {
    an_ifc_expr_index field = get_ifc_width(universal);

    db_print_indent(indent);
    fprintf(f_debug, "width:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_bitfield &universal)
/*
Given the universal representation of DeclBitfield, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "DeclBitfield ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_concept &universal, unsigned indent)
/*
Given the universal representation of DeclConcept, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_body(universal)) {
    an_ifc_sentence_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_chart(universal)) {
    an_ifc_chart_index field = get_ifc_chart(universal);

    db_print_indent(indent);
    fprintf(f_debug, "chart:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_constraint(universal)) {
    an_ifc_expr_index field = get_ifc_constraint(universal);

    db_print_indent(indent);
    fprintf(f_debug, "constraint:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_head(universal)) {
    an_ifc_sentence_index field = get_ifc_head(universal);

    db_print_indent(indent);
    fprintf(f_debug, "head: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_unknown(universal)) {
    an_ifc_u16 field = get_ifc_unknown(universal);

    db_print_indent(indent);
    fprintf(f_debug, "unknown: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_concept &universal)
/*
Given the universal representation of DeclConcept, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "DeclConcept ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_constructor &universal, unsigned indent)
/*
Given the universal representation of DeclConstructor, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_chart(universal)) {
    an_ifc_chart_index field = get_ifc_chart(universal);

    db_print_indent(indent);
    fprintf(f_debug, "chart:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_name_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_properties(universal)) {
    an_ifc_reachable_properties_bitfield field = get_ifc_properties(universal);

    fprintf(f_debug, "properties:\n");
    ++indent;
    if (test_bitmask<ifc_rpb_all>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- All\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_attributes>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Attributes\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_default_arguments>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DefaultArguments\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_initializer>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Initializer\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_traits(universal)) {
    an_ifc_function_traits_bitfield field = get_ifc_traits(universal);

    fprintf(f_debug, "traits:\n");
    ++indent;
    if (test_bitmask<ifc_ftb_constexpr>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Constexpr\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_constrained>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Constrained\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_defaulted>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Defaulted\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_deleted>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deleted\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_explicit>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Explicit\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_hidden_friend>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- HiddenFriend\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_immediate>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Immediate\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_inline>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Inline\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_no_return>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NoReturn\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_pure_virtual>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- PureVirtual\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_virtual>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Virtual\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_constructor &universal)
/*
Given the universal representation of DeclConstructor, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "DeclConstructor ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_deduction_guide &universal, unsigned indent)
/*
Given the universal representation of DeclDeductionGuide, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_source(universal)) {
    an_ifc_chart_index field = get_ifc_source(universal);

    db_print_indent(indent);
    fprintf(f_debug, "source:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_target(universal)) {
    an_ifc_expr_index field = get_ifc_target(universal);

    db_print_indent(indent);
    fprintf(f_debug, "target:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_traits(universal)) {
    an_ifc_guide_traits_bitfield field = get_ifc_traits(universal);

    db_print_indent(indent);
    fprintf(f_debug, "traits: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_deduction_guide &universal)
/*
Given the universal representation of DeclDeductionGuide, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "DeclDeductionGuide ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_default_argument &universal, unsigned indent)
/*
Given the universal representation of DeclDefaultArgument, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_properties(universal)) {
    an_ifc_reachable_properties_bitfield field = get_ifc_properties(universal);

    fprintf(f_debug, "properties:\n");
    ++indent;
    if (test_bitmask<ifc_rpb_all>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- All\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_attributes>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Attributes\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_default_arguments>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DefaultArguments\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_initializer>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Initializer\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_default_argument &universal)
/*
Given the universal representation of DeclDefaultArgument, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "DeclDefaultArgument ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_destructor &universal, unsigned indent)
/*
Given the universal representation of DeclDestructor, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_convention(universal)) {
    an_ifc_calling_convention_sort field = get_ifc_convention(universal);

    db_print_indent(indent);
    fprintf(f_debug, "convention: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_eh_spec(universal)) {
    an_ifc_noexcept_specification field = get_ifc_eh_spec(universal);

    db_print_indent(indent);
    fprintf(f_debug, "eh_spec:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_name_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_properties(universal)) {
    an_ifc_reachable_properties_bitfield field = get_ifc_properties(universal);

    fprintf(f_debug, "properties:\n");
    ++indent;
    if (test_bitmask<ifc_rpb_all>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- All\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_attributes>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Attributes\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_default_arguments>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DefaultArguments\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_initializer>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Initializer\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_traits(universal)) {
    an_ifc_function_traits_bitfield field = get_ifc_traits(universal);

    fprintf(f_debug, "traits:\n");
    ++indent;
    if (test_bitmask<ifc_ftb_constexpr>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Constexpr\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_constrained>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Constrained\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_defaulted>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Defaulted\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_deleted>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deleted\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_explicit>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Explicit\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_hidden_friend>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- HiddenFriend\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_immediate>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Immediate\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_inline>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Inline\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_no_return>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NoReturn\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_pure_virtual>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- PureVirtual\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_virtual>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Virtual\n");
    }  /* if */
    --indent;
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_destructor &universal)
/*
Given the universal representation of DeclDestructor, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "DeclDestructor ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_enumeration &universal, unsigned indent)
/*
Given the universal representation of DeclEnumeration, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_alignment(universal)) {
    an_ifc_expr_index field = get_ifc_alignment(universal);

    db_print_indent(indent);
    fprintf(f_debug, "alignment:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_base(universal)) {
    an_ifc_type_index field = get_ifc_base(universal);

    db_print_indent(indent);
    fprintf(f_debug, "base:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_sequence field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_properties(universal)) {
    an_ifc_reachable_properties_bitfield field = get_ifc_properties(universal);

    fprintf(f_debug, "properties:\n");
    ++indent;
    if (test_bitmask<ifc_rpb_all>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- All\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_attributes>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Attributes\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_default_arguments>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DefaultArguments\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_initializer>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Initializer\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_enumeration &universal)
/*
Given the universal representation of DeclEnumeration, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "DeclEnumeration ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_enumerator &universal, unsigned indent)
/*
Given the universal representation of DeclEnumerator, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_enumerator &universal)
/*
Given the universal representation of DeclEnumerator, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "DeclEnumerator ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_expansion &universal, unsigned indent)
/*
Given the universal representation of DeclExpansion, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operand(universal)) {
    an_ifc_decl_index field = get_ifc_operand(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operand:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_expansion &universal)
/*
Given the universal representation of DeclExpansion, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "DeclExpansion ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_explicit_instantiation &universal,
             unsigned                                 indent)
/*
Given the universal representation of DeclExplicitInstantiation, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_form(universal)) {
    an_ifc_form_spec_offset field = get_ifc_form(universal);

    db_print_indent(indent);
    fprintf(f_debug, "form: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_explicit_instantiation &universal)
/*
Given the universal representation of DeclExplicitInstantiation, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "DeclExplicitInstantiation ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_explicit_specialization &universal,
             unsigned                                  indent)
/*
Given the universal representation of DeclExplicitSpecialization, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_form(universal)) {
    an_ifc_form_spec_offset field = get_ifc_form(universal);

    db_print_indent(indent);
    fprintf(f_debug, "form: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_explicit_specialization &universal)
/*
Given the universal representation of DeclExplicitSpecialization, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "DeclExplicitSpecialization ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_field &universal, unsigned indent)
/*
Given the universal representation of DeclField, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_alignment(universal)) {
    an_ifc_expr_index field = get_ifc_alignment(universal);

    db_print_indent(indent);
    fprintf(f_debug, "alignment:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_properties(universal)) {
    an_ifc_reachable_properties_bitfield field = get_ifc_properties(universal);

    fprintf(f_debug, "properties:\n");
    ++indent;
    if (test_bitmask<ifc_rpb_all>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- All\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_attributes>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Attributes\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_default_arguments>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DefaultArguments\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_initializer>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Initializer\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_traits(universal)) {
    an_ifc_object_traits_bitfield field = get_ifc_traits(universal);

    fprintf(f_debug, "traits:\n");
    ++indent;
    if (test_bitmask<ifc_otb_constexpr>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Constexpr\n");
    }  /* if */
    if (test_bitmask<ifc_otb_initializer_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializerExported\n");
    }  /* if */
    if (test_bitmask<ifc_otb_inline>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Inline\n");
    }  /* if */
    if (test_bitmask<ifc_otb_mutable>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Mutable\n");
    }  /* if */
    if (test_bitmask<ifc_otb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_otb_thread_local>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- ThreadLocal\n");
    }  /* if */
    if (test_bitmask<ifc_otb_vendor>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vendor\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_field &universal)
/*
Given the universal representation of DeclField, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "DeclField ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_friend &universal, unsigned indent)
/*
Given the universal representation of DeclFriend, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_entity(universal)) {
    an_ifc_expr_index field = get_ifc_entity(universal);

    db_print_indent(indent);
    fprintf(f_debug, "entity:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_friend &universal)
/*
Given the universal representation of DeclFriend, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "DeclFriend ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_function &universal, unsigned indent)
/*
Given the universal representation of DeclFunction, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_chart(universal)) {
    an_ifc_chart_index field = get_ifc_chart(universal);

    db_print_indent(indent);
    fprintf(f_debug, "chart:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_name_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_properties(universal)) {
    an_ifc_reachable_properties_bitfield field = get_ifc_properties(universal);

    fprintf(f_debug, "properties:\n");
    ++indent;
    if (test_bitmask<ifc_rpb_all>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- All\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_attributes>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Attributes\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_default_arguments>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DefaultArguments\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_initializer>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Initializer\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_traits(universal)) {
    an_ifc_function_traits_bitfield field = get_ifc_traits(universal);

    fprintf(f_debug, "traits:\n");
    ++indent;
    if (test_bitmask<ifc_ftb_constexpr>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Constexpr\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_constrained>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Constrained\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_defaulted>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Defaulted\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_deleted>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deleted\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_explicit>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Explicit\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_hidden_friend>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- HiddenFriend\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_immediate>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Immediate\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_inline>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Inline\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_no_return>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NoReturn\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_pure_virtual>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- PureVirtual\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_virtual>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Virtual\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_function &universal)
/*
Given the universal representation of DeclFunction, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "DeclFunction ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_inherited_constructor &universal,
             unsigned                                indent)
/*
Given the universal representation of DeclInheritedConstructor, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_base_ctor(universal)) {
    an_ifc_decl_index field = get_ifc_base_ctor(universal);

    db_print_indent(indent);
    fprintf(f_debug, "base_ctor:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_chart(universal)) {
    an_ifc_chart_index field = get_ifc_chart(universal);

    db_print_indent(indent);
    fprintf(f_debug, "chart:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_traits(universal)) {
    an_ifc_function_traits_bitfield field = get_ifc_traits(universal);

    fprintf(f_debug, "traits:\n");
    ++indent;
    if (test_bitmask<ifc_ftb_constexpr>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Constexpr\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_constrained>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Constrained\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_defaulted>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Defaulted\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_deleted>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deleted\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_explicit>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Explicit\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_hidden_friend>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- HiddenFriend\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_immediate>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Immediate\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_inline>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Inline\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_no_return>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NoReturn\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_pure_virtual>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- PureVirtual\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_virtual>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Virtual\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_inherited_constructor &universal)
/*
Given the universal representation of DeclInheritedConstructor, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "DeclInheritedConstructor ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_intrinsic &universal, unsigned indent)
/*
Given the universal representation of DeclIntrinsic, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_intrinsic &universal)
/*
Given the universal representation of DeclIntrinsic, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "DeclIntrinsic ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_method &universal, unsigned indent)
/*
Given the universal representation of DeclMethod, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_chart(universal)) {
    an_ifc_chart_index field = get_ifc_chart(universal);

    db_print_indent(indent);
    fprintf(f_debug, "chart:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_name_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_properties(universal)) {
    an_ifc_reachable_properties_bitfield field = get_ifc_properties(universal);

    fprintf(f_debug, "properties:\n");
    ++indent;
    if (test_bitmask<ifc_rpb_all>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- All\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_attributes>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Attributes\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_default_arguments>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DefaultArguments\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_initializer>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Initializer\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_traits(universal)) {
    an_ifc_function_traits_bitfield field = get_ifc_traits(universal);

    fprintf(f_debug, "traits:\n");
    ++indent;
    if (test_bitmask<ifc_ftb_constexpr>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Constexpr\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_constrained>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Constrained\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_defaulted>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Defaulted\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_deleted>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deleted\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_explicit>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Explicit\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_hidden_friend>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- HiddenFriend\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_immediate>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Immediate\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_inline>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Inline\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_no_return>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NoReturn\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_pure_virtual>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- PureVirtual\n");
    }  /* if */
    if (test_bitmask<ifc_ftb_virtual>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Virtual\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_method &universal)
/*
Given the universal representation of DeclMethod, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "DeclMethod ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_output_segment &universal, unsigned indent)
/*
Given the universal representation of DeclOutputSegment, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_ID(universal)) {
    an_ifc_text_offset field = get_ifc_ID(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ID: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_traits(universal)) {
    an_ifc_segment_traits field = get_ifc_traits(universal);

    db_print_indent(indent);
    fprintf(f_debug, "traits: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_segment_type field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_output_segment &universal)
/*
Given the universal representation of DeclOutputSegment, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "DeclOutputSegment ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_parameter &universal, unsigned indent)
/*
Given the universal representation of DeclParameter, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_constraint(universal)) {
    an_ifc_expr_index field = get_ifc_constraint(universal);

    db_print_indent(indent);
    fprintf(f_debug, "constraint:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_init_decl(universal)) {
    an_ifc_expr_named_decl_offset field = get_ifc_init_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "init_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_level(universal)) {
    an_ifc_parameter_level field = get_ifc_level(universal);

    db_print_indent(indent);
    fprintf(f_debug, "level: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_pack(universal)) {
    an_ifc_bool field = get_ifc_pack(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pack: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_position(universal)) {
    an_ifc_parameter_position field = get_ifc_position(universal);

    db_print_indent(indent);
    fprintf(f_debug, "position: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_properties(universal)) {
    an_ifc_reachable_properties_bitfield field = get_ifc_properties(universal);

    fprintf(f_debug, "properties:\n");
    ++indent;
    if (test_bitmask<ifc_rpb_all>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- All\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_attributes>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Attributes\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_default_arguments>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DefaultArguments\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_initializer>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Initializer\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_sort(universal)) {
    an_ifc_parameter_sort field = get_ifc_sort(universal);

    db_print_indent(indent);
    fprintf(f_debug, "sort: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_parameter &universal)
/*
Given the universal representation of DeclParameter, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "DeclParameter ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_partial_specialization &universal,
             unsigned                                 indent)
/*
Given the universal representation of DeclPartialSpecialization, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_chart(universal)) {
    an_ifc_chart_index field = get_ifc_chart(universal);

    db_print_indent(indent);
    fprintf(f_debug, "chart:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_entity(universal)) {
    an_ifc_parameterized_entity field = get_ifc_entity(universal);

    db_print_indent(indent);
    fprintf(f_debug, "entity:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_form(universal)) {
    an_ifc_form_spec_offset field = get_ifc_form(universal);

    db_print_indent(indent);
    fprintf(f_debug, "form: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_name_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_primary_template(universal)) {
    an_ifc_decl_index field = get_ifc_primary_template(universal);

    db_print_indent(indent);
    fprintf(f_debug, "primary_template:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_properties(universal)) {
    an_ifc_reachable_properties_bitfield field = get_ifc_properties(universal);

    fprintf(f_debug, "properties:\n");
    ++indent;
    if (test_bitmask<ifc_rpb_all>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- All\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_attributes>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Attributes\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_default_arguments>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DefaultArguments\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_initializer>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Initializer\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_partial_specialization &universal)
/*
Given the universal representation of DeclPartialSpecialization, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "DeclPartialSpecialization ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_prolongation &universal, unsigned indent)
/*
Given the universal representation of DeclProlongation, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_enclosing_scope(universal)) {
    an_ifc_decl_index field = get_ifc_enclosing_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "enclosing_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_name_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_original_decl(universal)) {
    an_ifc_decl_index field = get_ifc_original_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "original_decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_prolongation &universal)
/*
Given the universal representation of DeclProlongation, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "DeclProlongation ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_property &universal, unsigned indent)
/*
Given the universal representation of DeclProperty, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_getter(universal)) {
    an_ifc_text_offset field = get_ifc_getter(universal);

    db_print_indent(indent);
    fprintf(f_debug, "getter: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_member(universal)) {
    an_ifc_decl_index field = get_ifc_member(universal);

    db_print_indent(indent);
    fprintf(f_debug, "member:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_setter(universal)) {
    an_ifc_text_offset field = get_ifc_setter(universal);

    db_print_indent(indent);
    fprintf(f_debug, "setter: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_property &universal)
/*
Given the universal representation of DeclProperty, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "DeclProperty ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_reference &universal, unsigned indent)
/*
Given the universal representation of DeclReference, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_index(universal)) {
    an_ifc_decl_index field = get_ifc_index(universal);

    db_print_indent(indent);
    fprintf(f_debug, "index:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_local_index(universal)) {
    db_print_indent(indent);
    fprintf(f_debug, "local_index: UNIMPLEMENTED\n");
  }  /* if */
  if (has_ifc_unit(universal)) {
    an_ifc_module_reference field = get_ifc_unit(universal);

    db_print_indent(indent);
    fprintf(f_debug, "unit:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_reference &universal)
/*
Given the universal representation of DeclReference, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "DeclReference ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_scope &universal, unsigned indent)
/*
Given the universal representation of DeclScope, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_alignment(universal)) {
    an_ifc_expr_index field = get_ifc_alignment(universal);

    db_print_indent(indent);
    fprintf(f_debug, "alignment:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_base(universal)) {
    an_ifc_type_index field = get_ifc_base(universal);

    db_print_indent(indent);
    fprintf(f_debug, "base:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_scope_offset field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_name_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_pack_size(universal)) {
    an_ifc_pack_size field = get_ifc_pack_size(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pack_size: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_properties(universal)) {
    an_ifc_reachable_properties_bitfield field = get_ifc_properties(universal);

    fprintf(f_debug, "properties:\n");
    ++indent;
    if (test_bitmask<ifc_rpb_all>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- All\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_attributes>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Attributes\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_default_arguments>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DefaultArguments\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_initializer>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Initializer\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_traits(universal)) {
    an_ifc_scope_traits_bitfield field = get_ifc_traits(universal);

    fprintf(f_debug, "traits:\n");
    ++indent;
    if (test_bitmask<ifc_stb_closure_type>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- ClosureType\n");
    }  /* if */
    if (test_bitmask<ifc_stb_final>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Final\n");
    }  /* if */
    if (test_bitmask<ifc_stb_initializer_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializerExported\n");
    }  /* if */
    if (test_bitmask<ifc_stb_inline>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Inline\n");
    }  /* if */
    if (test_bitmask<ifc_stb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_stb_unnamed>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Unnamed\n");
    }  /* if */
    if (test_bitmask<ifc_stb_vendor>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vendor\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_scope &universal)
/*
Given the universal representation of DeclScope, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "DeclScope ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_specialization &universal, unsigned indent)
/*
Given the universal representation of DeclSpecialization, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_form(universal)) {
    an_ifc_form_spec_offset field = get_ifc_form(universal);

    db_print_indent(indent);
    fprintf(f_debug, "form: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_name_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_primary_template(universal)) {
    an_ifc_decl_index field = get_ifc_primary_template(universal);

    db_print_indent(indent);
    fprintf(f_debug, "primary_template:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_sort(universal)) {
    an_ifc_specialization_sort field = get_ifc_sort(universal);

    db_print_indent(indent);
    fprintf(f_debug, "sort: %s\n", str_for(field));
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_specialization &universal)
/*
Given the universal representation of DeclSpecialization, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "DeclSpecialization ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_syntax_tree &universal, unsigned indent)
/*
Given the universal representation of DeclSyntaxTree, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_syntax(universal)) {
    an_ifc_syntax_index field = get_ifc_syntax(universal);

    db_print_indent(indent);
    fprintf(f_debug, "syntax:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_syntax_tree &universal)
/*
Given the universal representation of DeclSyntaxTree, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "DeclSyntaxTree ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_template &universal, unsigned indent)
/*
Given the universal representation of DeclTemplate, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_chart(universal)) {
    an_ifc_chart_index field = get_ifc_chart(universal);

    db_print_indent(indent);
    fprintf(f_debug, "chart:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_entity(universal)) {
    an_ifc_parameterized_entity field = get_ifc_entity(universal);

    db_print_indent(indent);
    fprintf(f_debug, "entity:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_name_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_properties(universal)) {
    an_ifc_reachable_properties_bitfield field = get_ifc_properties(universal);

    fprintf(f_debug, "properties:\n");
    ++indent;
    if (test_bitmask<ifc_rpb_all>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- All\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_attributes>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Attributes\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_default_arguments>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DefaultArguments\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_initializer>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Initializer\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_template &universal)
/*
Given the universal representation of DeclTemplate, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "DeclTemplate ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_temploid &universal, unsigned indent)
/*
Given the universal representation of DeclTemploid, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_chart(universal)) {
    an_ifc_chart_index field = get_ifc_chart(universal);

    db_print_indent(indent);
    fprintf(f_debug, "chart:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_entity(universal)) {
    an_ifc_parameterized_entity field = get_ifc_entity(universal);

    db_print_indent(indent);
    fprintf(f_debug, "entity:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_properties(universal)) {
    an_ifc_reachable_properties_bitfield field = get_ifc_properties(universal);

    fprintf(f_debug, "properties:\n");
    ++indent;
    if (test_bitmask<ifc_rpb_all>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- All\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_attributes>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Attributes\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_default_arguments>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DefaultArguments\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_initializer>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Initializer\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    --indent;
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_temploid &universal)
/*
Given the universal representation of DeclTemploid, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "DeclTemploid ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_tuple &universal, unsigned indent)
/*
Given the universal representation of DeclTuple, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_tuple &universal)
/*
Given the universal representation of DeclTuple, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "DeclTuple ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_using_declaration &universal, unsigned indent)
/*
Given the universal representation of DeclUsingDeclaration, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_hidden(universal)) {
    an_ifc_bool field = get_ifc_hidden(universal);

    db_print_indent(indent);
    fprintf(f_debug, "hidden: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_name2(universal)) {
    an_ifc_text_offset field = get_ifc_name2(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name2: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_parent(universal)) {
    an_ifc_expr_index field = get_ifc_parent(universal);

    db_print_indent(indent);
    fprintf(f_debug, "parent:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_resolution(universal)) {
    an_ifc_decl_index field = get_ifc_resolution(universal);

    db_print_indent(indent);
    fprintf(f_debug, "resolution:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_using_declaration &universal)
/*
Given the universal representation of DeclUsingDeclaration, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "DeclUsingDeclaration ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_decl_variable &universal, unsigned indent)
/*
Given the universal representation of DeclVariable, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_alignment(universal)) {
    an_ifc_expr_index field = get_ifc_alignment(universal);

    db_print_indent(indent);
    fprintf(f_debug, "alignment:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_home_scope(universal)) {
    an_ifc_decl_index field = get_ifc_home_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "home_scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_name_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_properties(universal)) {
    an_ifc_reachable_properties_bitfield field = get_ifc_properties(universal);

    fprintf(f_debug, "properties:\n");
    ++indent;
    if (test_bitmask<ifc_rpb_all>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- All\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_attributes>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Attributes\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_default_arguments>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DefaultArguments\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_initializer>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Initializer\n");
    }  /* if */
    if (test_bitmask<ifc_rpb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_basic_specifiers_bitfield field = get_ifc_specifiers(universal);

    fprintf(f_debug, "specifiers:\n");
    ++indent;
    if (test_bitmask<ifc_bsb_c>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- C\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_cxx>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Cxx\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Deprecated\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- External\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializedInClass\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Internal\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_is_member_of_global_module>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IsMemberOfGlobalModule\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NonExported\n");
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vague\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_traits(universal)) {
    an_ifc_object_traits_bitfield field = get_ifc_traits(universal);

    fprintf(f_debug, "traits:\n");
    ++indent;
    if (test_bitmask<ifc_otb_constexpr>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Constexpr\n");
    }  /* if */
    if (test_bitmask<ifc_otb_initializer_exported>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- InitializerExported\n");
    }  /* if */
    if (test_bitmask<ifc_otb_inline>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Inline\n");
    }  /* if */
    if (test_bitmask<ifc_otb_mutable>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Mutable\n");
    }  /* if */
    if (test_bitmask<ifc_otb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_otb_thread_local>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- ThreadLocal\n");
    }  /* if */
    if (test_bitmask<ifc_otb_vendor>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Vendor\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_decl_variable &universal)
/*
Given the universal representation of DeclVariable, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "DeclVariable ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_dir_attribute &universal, unsigned indent)
/*
Given the universal representation of DirAttribute, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_attr(universal)) {
    an_ifc_attr_index field = get_ifc_attr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "attr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_dir_attribute &universal)
/*
Given the universal representation of DirAttribute, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "DirAttribute ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_dir_decl_use &universal, unsigned indent)
/*
Given the universal representation of DirDeclUse, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_path(universal)) {
    an_ifc_expr_index field = get_ifc_path(universal);

    db_print_indent(indent);
    fprintf(f_debug, "path:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_result(universal)) {
    an_ifc_decl_index field = get_ifc_result(universal);

    db_print_indent(indent);
    fprintf(f_debug, "result:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_dir_decl_use &universal)
/*
Given the universal representation of DirDeclUse, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "DirDeclUse ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_dir_empty &universal, unsigned indent)
/*
Given the universal representation of DirEmpty, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_dir_empty &universal)
/*
Given the universal representation of DirEmpty, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "DirEmpty ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_dir_expr &universal, unsigned indent)
/*
Given the universal representation of DirExpr, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_expr(universal)) {
    an_ifc_expr_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_phases(universal)) {
    an_ifc_phases_bitfield field = get_ifc_phases(universal);

    fprintf(f_debug, "phases:\n");
    ++indent;
    if (test_bitmask<ifc_pb_analysis>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Analysis\n");
    }  /* if */
    if (test_bitmask<ifc_pb_code_generation>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- CodeGeneration\n");
    }  /* if */
    if (test_bitmask<ifc_pb_evaluation>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Evaluation\n");
    }  /* if */
    if (test_bitmask<ifc_pb_execution>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Execution\n");
    }  /* if */
    if (test_bitmask<ifc_pb_importing>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Importing\n");
    }  /* if */
    if (test_bitmask<ifc_pb_instantiation>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Instantiation\n");
    }  /* if */
    if (test_bitmask<ifc_pb_lexing>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Lexing\n");
    }  /* if */
    if (test_bitmask<ifc_pb_linking>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Linking\n");
    }  /* if */
    if (test_bitmask<ifc_pb_loading>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Loading\n");
    }  /* if */
    if (test_bitmask<ifc_pb_name_resolution>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NameResolution\n");
    }  /* if */
    if (test_bitmask<ifc_pb_parsing>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Parsing\n");
    }  /* if */
    if (test_bitmask<ifc_pb_preprocessing>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Preprocessing\n");
    }  /* if */
    if (test_bitmask<ifc_pb_reading>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Reading\n");
    }  /* if */
    if (test_bitmask<ifc_pb_typing>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Typing\n");
    }  /* if */
    if (test_bitmask<ifc_pb_unknown>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Unknown\n");
    }  /* if */
    --indent;
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_dir_expr &universal)
/*
Given the universal representation of DirExpr, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "DirExpr ");
  fprintf(f_debug, "====================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_dir_pragma &universal, unsigned indent)
/*
Given the universal representation of DirPragma, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_words(universal)) {
    an_ifc_sentence_index field = get_ifc_words(universal);

    db_print_indent(indent);
    fprintf(f_debug, "words: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_dir_pragma &universal)
/*
Given the universal representation of DirPragma, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "DirPragma ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_dir_tuple &universal, unsigned indent)
/*
Given the universal representation of DirTuple, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_dir_tuple &universal)
/*
Given the universal representation of DirTuple, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "DirTuple ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_dir_using &universal, unsigned indent)
/*
Given the universal representation of DirUsing, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_nominated(universal)) {
    an_ifc_expr_index field = get_ifc_nominated(universal);

    db_print_indent(indent);
    fprintf(f_debug, "nominated:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_resolution(universal)) {
    an_ifc_decl_index field = get_ifc_resolution(universal);

    db_print_indent(indent);
    fprintf(f_debug, "resolution:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_dir_using &universal)
/*
Given the universal representation of DirUsing, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "DirUsing ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_constant_integer &universal, unsigned indent)
/*
Given the universal representation of EdgConstantInteger, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_edg_constant_integer_word_offset field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_constant_integer &universal)
/*
Given the universal representation of EdgConstantInteger, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "EdgConstantInteger ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_constant_integer_word &universal,
             unsigned                               indent)
/*
Given the universal representation of EdgConstantIntegerWord, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_bytes(universal)) {
    db_print_indent(indent);
    fprintf(f_debug, "bytes: UNIMPLEMENTED\n");
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_constant_integer_word &universal)
/*
Given the universal representation of EdgConstantIntegerWord, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "EdgConstantIntegerWord ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_expr_template_argument &universal,
             unsigned                                indent)
/*
Given the universal representation of EdgExprTemplateArgument, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_index(universal)) {
    an_ifc_edg_template_argument_index field = get_ifc_index(universal);

    db_print_indent(indent);
    fprintf(f_debug, "index:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_expr_template_argument &universal)
/*
Given the universal representation of EdgExprTemplateArgument, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "EdgExprTemplateArgument ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_extension_expr &universal, unsigned indent)
/*
Given the universal representation of EdgExtensionExpr, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_edg_expr_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_extension_expr &universal)
/*
Given the universal representation of EdgExtensionExpr, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "EdgExtensionExpr ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_extension_type &universal, unsigned indent)
/*
Given the universal representation of EdgExtensionType, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_edg_type_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_extension_type &universal)
/*
Given the universal representation of EdgExtensionType, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "EdgExtensionType ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_heap_complex_token &universal, unsigned indent)
/*
Given the universal representation of EdgHeapComplexToken, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_index(universal)) {
    an_ifc_edg_complex_token_index field = get_ifc_index(universal);

    db_print_indent(indent);
    fprintf(f_debug, "index:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_heap_complex_token &universal)
/*
Given the universal representation of EdgHeapComplexToken, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "EdgHeapComplexToken ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_heap_template_argument &universal,
             unsigned                                indent)
/*
Given the universal representation of EdgHeapTemplateArgument, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_index(universal)) {
    an_ifc_edg_template_argument_index field = get_ifc_index(universal);

    db_print_indent(indent);
    fprintf(f_debug, "index:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_heap_template_argument &universal)
/*
Given the universal representation of EdgHeapTemplateArgument, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "EdgHeapTemplateArgument ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_template_argument_constant &universal,
             unsigned                                    indent)
/*
Given the universal representation of EdgTemplateArgumentConstant, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_edg_constant_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_template_argument_constant &universal)
/*
Given the universal representation of EdgTemplateArgumentConstant, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================= ");
  fprintf(f_debug, "EdgTemplateArgumentConstant ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_template_argument_non_type &universal,
             unsigned                                    indent)
/*
Given the universal representation of EdgTemplateArgumentNonType, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_expr_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_template_argument_non_type &universal)
/*
Given the universal representation of EdgTemplateArgumentNonType, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "EdgTemplateArgumentNonType ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_template_argument_template &universal,
             unsigned                                    indent)
/*
Given the universal representation of EdgTemplateArgumentTemplate, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_decl_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_template_argument_template &universal)
/*
Given the universal representation of EdgTemplateArgumentTemplate, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================= ");
  fprintf(f_debug, "EdgTemplateArgumentTemplate ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_template_argument_type &universal,
             unsigned                                indent)
/*
Given the universal representation of EdgTemplateArgumentType, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_type_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_template_argument_type &universal)
/*
Given the universal representation of EdgTemplateArgumentType, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "EdgTemplateArgumentType ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_token_basic &universal, unsigned indent)
/*
Given the universal representation of EdgTokenBasic, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_kind(universal)) {
    an_ifc_edg_basic_token_sort field = get_ifc_kind(universal);

    db_print_indent(indent);
    fprintf(f_debug, "kind: %s\n", str_for(field));
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_token_basic &universal)
/*
Given the universal representation of EdgTokenBasic, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "EdgTokenBasic ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_token_cache &universal, unsigned indent)
/*
Given the universal representation of EdgTokenCache, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_complex_tokens(universal)) {
    an_ifc_edg_heap_complex_token_offset field =
                                             get_ifc_complex_tokens(universal);

    db_print_indent(indent);
    fprintf(f_debug, "complex_tokens: %llu\n",
            (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_num_complex_tokens(universal)) {
    an_ifc_cardinality field = get_ifc_num_complex_tokens(universal);

    db_print_indent(indent);
    fprintf(f_debug, "num_complex_tokens: %llu\n",
            (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_num_tokens(universal)) {
    an_ifc_cardinality field = get_ifc_num_tokens(universal);

    db_print_indent(indent);
    fprintf(f_debug, "num_tokens: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_tokens(universal)) {
    an_ifc_edg_token_basic_offset field = get_ifc_tokens(universal);

    db_print_indent(indent);
    fprintf(f_debug, "tokens: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_token_cache &universal)
/*
Given the universal representation of EdgTokenCache, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "EdgTokenCache ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_token_constant &universal, unsigned indent)
/*
Given the universal representation of EdgTokenConstant, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_constant(universal)) {
    an_ifc_edg_constant_index field = get_ifc_constant(universal);

    db_print_indent(indent);
    fprintf(f_debug, "constant:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_kind(universal)) {
    an_ifc_edg_constant_token_sort field = get_ifc_kind(universal);

    db_print_indent(indent);
    fprintf(f_debug, "kind: %s\n", str_for(field));
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_token_constant &universal)
/*
Given the universal representation of EdgTokenConstant, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "EdgTokenConstant ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_token_identifier &universal, unsigned indent)
/*
Given the universal representation of EdgTokenIdentifier, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_text(universal)) {
    an_ifc_text_offset field = get_ifc_text(universal);

    db_print_indent(indent);
    fprintf(f_debug, "text: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_token_identifier &universal)
/*
Given the universal representation of EdgTokenIdentifier, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "EdgTokenIdentifier ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_token_textual &universal, unsigned indent)
/*
Given the universal representation of EdgTokenTextual, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_characters(universal)) {
    an_ifc_text_offset field = get_ifc_characters(universal);

    db_print_indent(indent);
    fprintf(f_debug, "characters: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_token_textual &universal)
/*
Given the universal representation of EdgTokenTextual, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "EdgTokenTextual ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_trait_class_template_definition &universal,
             unsigned                                         indent)
/*
Given the universal representation of EdgTraitClassTemplateDefinition, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_encoded_decl(universal)) {
    an_ifc_encoded_decl_index field = get_ifc_encoded_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_edg_token_cache_offset field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_trait_class_template_definition &universal)
/*
Given the universal representation of EdgTraitClassTemplateDefinition, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "======================= ");
  fprintf(f_debug, "EdgTraitClassTemplateDefinition ");
  fprintf(f_debug, "========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_trait_function_definition &universal,
             unsigned                                   indent)
/*
Given the universal representation of EdgTraitFunctionDefinition, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_encoded_decl(universal)) {
    an_ifc_encoded_decl_index field = get_ifc_encoded_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_edg_token_cache_offset field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_trait_function_definition &universal)
/*
Given the universal representation of EdgTraitFunctionDefinition, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "EdgTraitFunctionDefinition ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_type_float &universal, unsigned indent)
/*
Given the universal representation of EdgTypeFloat, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_kind(universal)) {
    an_ifc_edg_float_type_sort field = get_ifc_kind(universal);

    db_print_indent(indent);
    fprintf(f_debug, "kind: %s\n", str_for(field));
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_type_float &universal)
/*
Given the universal representation of EdgTypeFloat, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "EdgTypeFloat ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_edg_type_substituted &universal, unsigned indent)
/*
Given the universal representation of EdgTypeSubstituted, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_arguments(universal)) {
    an_ifc_edg_heap_template_argument_offset field =
                                                  get_ifc_arguments(universal);

    db_print_indent(indent);
    fprintf(f_debug, "arguments: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_num_arguments(universal)) {
    an_ifc_cardinality field = get_ifc_num_arguments(universal);

    db_print_indent(indent);
    fprintf(f_debug, "num_arguments: %llu\n",
            (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_subject(universal)) {
    an_ifc_decl_index field = get_ifc_subject(universal);

    db_print_indent(indent);
    fprintf(f_debug, "subject:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_edg_type_substituted &universal)
/*
Given the universal representation of EdgTypeSubstituted, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "EdgTypeSubstituted ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_alignof &universal, unsigned indent)
/*
Given the universal representation of ExprAlignof, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operand(universal)) {
    an_ifc_syntax_index field = get_ifc_operand(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operand:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_alignof &universal)
/*
Given the universal representation of ExprAlignof, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "ExprAlignof ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_array_value &universal, unsigned indent)
/*
Given the universal representation of ExprArrayValue, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_element_type(universal)) {
    an_ifc_type_index field = get_ifc_element_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "element_type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_elements(universal)) {
    an_ifc_expr_index field = get_ifc_elements(universal);

    db_print_indent(indent);
    fprintf(f_debug, "elements:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_array_value &universal)
/*
Given the universal representation of ExprArrayValue, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "ExprArrayValue ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_assign_initializer &universal, unsigned indent)
/*
Given the universal representation of ExprAssignInitializer, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_equal(universal)) {
    an_ifc_source_location field = get_ifc_equal(universal);

    db_print_indent(indent);
    fprintf(f_debug, "equal:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_assign_initializer &universal)
/*
Given the universal representation of ExprAssignInitializer, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "ExprAssignInitializer ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_binary_fold &universal, unsigned indent)
/*
Given the universal representation of ExprBinaryFold, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_associativity(universal)) {
    an_ifc_associativity field = get_ifc_associativity(universal);

    db_print_indent(indent);
    fprintf(f_debug, "associativity: %llu\n",
            (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_left(universal)) {
    an_ifc_expr_index field = get_ifc_left(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operation(universal)) {
    an_ifc_dyadic_operator_sort field = get_ifc_operation(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operation: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_right(universal)) {
    an_ifc_expr_index field = get_ifc_right(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_binary_fold &universal)
/*
Given the universal representation of ExprBinaryFold, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "ExprBinaryFold ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_call &universal, unsigned indent)
/*
Given the universal representation of ExprCall, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_arguments(universal)) {
    an_ifc_expr_index field = get_ifc_arguments(universal);

    db_print_indent(indent);
    fprintf(f_debug, "arguments:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operation(universal)) {
    an_ifc_expr_index field = get_ifc_operation(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operation:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_call &universal)
/*
Given the universal representation of ExprCall, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "ExprCall ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_cast &universal, unsigned indent)
/*
Given the universal representation of ExprCast, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_op(universal)) {
    an_ifc_dyadic_operator_sort field = get_ifc_op(universal);

    db_print_indent(indent);
    fprintf(f_debug, "op: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_source(universal)) {
    an_ifc_expr_index field = get_ifc_source(universal);

    db_print_indent(indent);
    fprintf(f_debug, "source:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_target(universal)) {
    an_ifc_type_index field = get_ifc_target(universal);

    db_print_indent(indent);
    fprintf(f_debug, "target:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_cast &universal)
/*
Given the universal representation of ExprCast, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "ExprCast ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_compound_string &universal, unsigned indent)
/*
Given the universal representation of ExprCompoundString, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_prefix(universal)) {
    an_ifc_text_offset field = get_ifc_prefix(universal);

    db_print_indent(indent);
    fprintf(f_debug, "prefix: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_string(universal)) {
    an_ifc_expr_index field = get_ifc_string(universal);

    db_print_indent(indent);
    fprintf(f_debug, "string:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_compound_string &universal)
/*
Given the universal representation of ExprCompoundString, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "ExprCompoundString ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_condition &universal, unsigned indent)
/*
Given the universal representation of ExprCondition, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_expr(universal)) {
    an_ifc_expr_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_condition &universal)
/*
Given the universal representation of ExprCondition, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "ExprCondition ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_designated_initializer &universal,
             unsigned                                 indent)
/*
Given the universal representation of ExprDesignatedInitializer, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_member(universal)) {
    an_ifc_text_offset field = get_ifc_member(universal);

    db_print_indent(indent);
    fprintf(f_debug, "member: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_designated_initializer &universal)
/*
Given the universal representation of ExprDesignatedInitializer, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "ExprDesignatedInitializer ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_destructor_call &universal, unsigned indent)
/*
Given the universal representation of ExprDestructorCall, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_cleanup(universal)) {
    an_ifc_destructor_sort field = get_ifc_cleanup(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cleanup: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_decltype_specifier(universal)) {
    an_ifc_syntax_index field = get_ifc_decltype_specifier(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decltype_specifier:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_expr_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_destructor_call &universal)
/*
Given the universal representation of ExprDestructorCall, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "ExprDestructorCall ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_dyad &universal, unsigned indent)
/*
Given the universal representation of ExprDyad, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_argument_0(universal)) {
    an_ifc_expr_index field = get_ifc_argument_0(universal);

    db_print_indent(indent);
    fprintf(f_debug, "argument_0:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_argument_1(universal)) {
    an_ifc_expr_index field = get_ifc_argument_1(universal);

    db_print_indent(indent);
    fprintf(f_debug, "argument_1:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_assoc(universal)) {
    an_ifc_dyadic_operator_sort field = get_ifc_assoc(universal);

    db_print_indent(indent);
    fprintf(f_debug, "assoc: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_impl(universal)) {
    an_ifc_decl_index field = get_ifc_impl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "impl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_dyad &universal)
/*
Given the universal representation of ExprDyad, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "ExprDyad ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_dynamic_dispatch &universal, unsigned indent)
/*
Given the universal representation of ExprDynamicDispatch, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_pivot(universal)) {
    an_ifc_expr_index field = get_ifc_pivot(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pivot:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_dynamic_dispatch &universal)
/*
Given the universal representation of ExprDynamicDispatch, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "ExprDynamicDispatch ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_empty &universal, unsigned indent)
/*
Given the universal representation of ExprEmpty, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_empty &universal)
/*
Given the universal representation of ExprEmpty, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "ExprEmpty ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_expansion &universal, unsigned indent)
/*
Given the universal representation of ExprExpansion, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operand(universal)) {
    an_ifc_expr_index field = get_ifc_operand(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operand:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_expansion &universal)
/*
Given the universal representation of ExprExpansion, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "ExprExpansion ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_expression_list &universal, unsigned indent)
/*
Given the universal representation of ExprExpressionList, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_contents(universal)) {
    an_ifc_expr_index field = get_ifc_contents(universal);

    db_print_indent(indent);
    fprintf(f_debug, "contents:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_delimiter(universal)) {
    an_ifc_delimiter_sort field = get_ifc_delimiter(universal);

    db_print_indent(indent);
    fprintf(f_debug, "delimiter: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_left(universal)) {
    an_ifc_source_location field = get_ifc_left(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_right(universal)) {
    an_ifc_source_location field = get_ifc_right(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_expression_list &universal)
/*
Given the universal representation of ExprExpressionList, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "ExprExpressionList ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_function_string &universal, unsigned indent)
/*
Given the universal representation of ExprFunctionString, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_macro(universal)) {
    an_ifc_text_offset field = get_ifc_macro(universal);

    db_print_indent(indent);
    fprintf(f_debug, "macro: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_function_string &universal)
/*
Given the universal representation of ExprFunctionString, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "ExprFunctionString ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_hierarchy_conversion &universal,
             unsigned                               indent)
/*
Given the universal representation of ExprHierarchyConversion, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_inheritance(universal)) {
    an_ifc_expr_index field = get_ifc_inheritance(universal);

    db_print_indent(indent);
    fprintf(f_debug, "inheritance:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_op(universal)) {
    an_ifc_dyadic_operator_sort field = get_ifc_op(universal);

    db_print_indent(indent);
    fprintf(f_debug, "op: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_override(universal)) {
    an_ifc_expr_index field = get_ifc_override(universal);

    db_print_indent(indent);
    fprintf(f_debug, "override:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_source(universal)) {
    an_ifc_expr_index field = get_ifc_source(universal);

    db_print_indent(indent);
    fprintf(f_debug, "source:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_target(universal)) {
    an_ifc_type_index field = get_ifc_target(universal);

    db_print_indent(indent);
    fprintf(f_debug, "target:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_hierarchy_conversion &universal)
/*
Given the universal representation of ExprHierarchyConversion, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "ExprHierarchyConversion ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_inheritance_path &universal, unsigned indent)
/*
Given the universal representation of ExprInheritancePath, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_path(universal)) {
    an_ifc_expr_index field = get_ifc_path(universal);

    db_print_indent(indent);
    fprintf(f_debug, "path:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_inheritance_path &universal)
/*
Given the universal representation of ExprInheritancePath, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "ExprInheritancePath ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_initializer &universal, unsigned indent)
/*
Given the universal representation of ExprInitializer, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_expr(universal)) {
    an_ifc_expr_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_sort(universal)) {
    an_ifc_initializer_sort field = get_ifc_sort(universal);

    db_print_indent(indent);
    fprintf(f_debug, "sort: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_initializer &universal)
/*
Given the universal representation of ExprInitializer, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "ExprInitializer ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_initializer_list &universal, unsigned indent)
/*
Given the universal representation of ExprInitializerList, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_elements(universal)) {
    an_ifc_expr_index field = get_ifc_elements(universal);

    db_print_indent(indent);
    fprintf(f_debug, "elements:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_initializer_list &universal)
/*
Given the universal representation of ExprInitializerList, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "ExprInitializerList ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_label &universal, unsigned indent)
/*
Given the universal representation of ExprLabel, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_designator(universal)) {
    an_ifc_expr_index field = get_ifc_designator(universal);

    db_print_indent(indent);
    fprintf(f_debug, "designator:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_label &universal)
/*
Given the universal representation of ExprLabel, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "ExprLabel ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_lambda &universal, unsigned indent)
/*
Given the universal representation of ExprLambda, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_syntax_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_constraint(universal)) {
    an_ifc_syntax_index field = get_ifc_constraint(universal);

    db_print_indent(indent);
    fprintf(f_debug, "constraint:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_declarator(universal)) {
    an_ifc_syntax_index field = get_ifc_declarator(universal);

    db_print_indent(indent);
    fprintf(f_debug, "declarator:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_introducer(universal)) {
    an_ifc_syntax_index field = get_ifc_introducer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "introducer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_template_parameters(universal)) {
    an_ifc_syntax_index field = get_ifc_template_parameters(universal);

    db_print_indent(indent);
    fprintf(f_debug, "template_parameters:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_lambda &universal)
/*
Given the universal representation of ExprLambda, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "ExprLambda ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_literal &universal, unsigned indent)
/*
Given the universal representation of ExprLiteral, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_value(universal)) {
    an_ifc_lit_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_literal &universal)
/*
Given the universal representation of ExprLiteral, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "ExprLiteral ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_member_access &universal, unsigned indent)
/*
Given the universal representation of ExprMemberAccess, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_enclosing(universal)) {
    an_ifc_type_index field = get_ifc_enclosing(universal);

    db_print_indent(indent);
    fprintf(f_debug, "enclosing:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_offset(universal)) {
    an_ifc_expr_index field = get_ifc_offset(universal);

    db_print_indent(indent);
    fprintf(f_debug, "offset:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_member_access &universal)
/*
Given the universal representation of ExprMemberAccess, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "ExprMemberAccess ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_member_initializer &universal, unsigned indent)
/*
Given the universal representation of ExprMemberInitializer, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_base(universal)) {
    an_ifc_type_index field = get_ifc_base(universal);

    db_print_indent(indent);
    fprintf(f_debug, "base:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_member(universal)) {
    an_ifc_decl_index field = get_ifc_member(universal);

    db_print_indent(indent);
    fprintf(f_debug, "member:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_member_initializer &universal)
/*
Given the universal representation of ExprMemberInitializer, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "ExprMemberInitializer ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_monad &universal, unsigned indent)
/*
Given the universal representation of ExprMonad, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_argument(universal)) {
    an_ifc_expr_index field = get_ifc_argument(universal);

    db_print_indent(indent);
    fprintf(f_debug, "argument:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_assoc(universal)) {
    an_ifc_monadic_operator_sort field = get_ifc_assoc(universal);

    db_print_indent(indent);
    fprintf(f_debug, "assoc: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_impl(universal)) {
    an_ifc_decl_index field = get_ifc_impl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "impl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_monad &universal)
/*
Given the universal representation of ExprMonad, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "ExprMonad ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_named_decl &universal, unsigned indent)
/*
Given the universal representation of ExprNamedDecl, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_resolution(universal)) {
    an_ifc_decl_index field = get_ifc_resolution(universal);

    db_print_indent(indent);
    fprintf(f_debug, "resolution:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_named_decl &universal)
/*
Given the universal representation of ExprNamedDecl, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "ExprNamedDecl ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_nullptr &universal, unsigned indent)
/*
Given the universal representation of ExprNullptr, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_nullptr &universal)
/*
Given the universal representation of ExprNullptr, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "ExprNullptr ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_packed_template_arguments &universal,
             unsigned                                    indent)
/*
Given the universal representation of ExprPackedTemplateArguments, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_arguments(universal)) {
    an_ifc_expr_index field = get_ifc_arguments(universal);

    db_print_indent(indent);
    fprintf(f_debug, "arguments:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_packed_template_arguments &universal)
/*
Given the universal representation of ExprPackedTemplateArguments, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================= ");
  fprintf(f_debug, "ExprPackedTemplateArguments ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_path &universal, unsigned indent)
/*
Given the universal representation of ExprPath, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_member(universal)) {
    an_ifc_expr_index field = get_ifc_member(universal);

    db_print_indent(indent);
    fprintf(f_debug, "member:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_scope(universal)) {
    an_ifc_expr_index field = get_ifc_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_path &universal)
/*
Given the universal representation of ExprPath, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "ExprPath ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_placeholder &universal, unsigned indent)
/*
Given the universal representation of ExprPlaceholder, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_placeholder &universal)
/*
Given the universal representation of ExprPlaceholder, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "ExprPlaceholder ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_pointer &universal, unsigned indent)
/*
Given the universal representation of ExprPointer, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_pointer &universal)
/*
Given the universal representation of ExprPointer, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "ExprPointer ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_product_type_value &universal, unsigned indent)
/*
Given the universal representation of ExprProductTypeValue, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_base_subobjects(universal)) {
    an_ifc_expr_index field = get_ifc_base_subobjects(universal);

    db_print_indent(indent);
    fprintf(f_debug, "base_subobjects:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_class_decl(universal)) {
    an_ifc_type_index field = get_ifc_class_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "class_decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_members(universal)) {
    an_ifc_expr_index field = get_ifc_members(universal);

    db_print_indent(indent);
    fprintf(f_debug, "members:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_product_type_value &universal)
/*
Given the universal representation of ExprProductTypeValue, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "ExprProductTypeValue ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_push_state &universal, unsigned indent)
/*
Given the universal representation of ExprPushState, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_ctor_call(universal)) {
    an_ifc_expr_index field = get_ifc_ctor_call(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ctor_call:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_dtor_call(universal)) {
    an_ifc_expr_index field = get_ifc_dtor_call(universal);

    db_print_indent(indent);
    fprintf(f_debug, "dtor_call:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_flags(universal)) {
    an_ifc_eh_flags field = get_ifc_flags(universal);

    db_print_indent(indent);
    fprintf(f_debug, "flags: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_push_state &universal)
/*
Given the universal representation of ExprPushState, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "ExprPushState ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_qualified_name &universal, unsigned indent)
/*
Given the universal representation of ExprQualifiedName, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_elements(universal)) {
    an_ifc_expr_index field = get_ifc_elements(universal);

    db_print_indent(indent);
    fprintf(f_debug, "elements:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_typename_keyword(universal)) {
    an_ifc_source_location field = get_ifc_typename_keyword(universal);

    db_print_indent(indent);
    fprintf(f_debug, "typename_keyword:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_qualified_name &universal)
/*
Given the universal representation of ExprQualifiedName, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "ExprQualifiedName ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_read &universal, unsigned indent)
/*
Given the universal representation of ExprRead, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_address(universal)) {
    an_ifc_expr_index field = get_ifc_address(universal);

    db_print_indent(indent);
    fprintf(f_debug, "address:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_sort(universal)) {
    an_ifc_read_conversion_sort field = get_ifc_sort(universal);

    db_print_indent(indent);
    fprintf(f_debug, "sort: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_read &universal)
/*
Given the universal representation of ExprRead, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "ExprRead ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_requires &universal, unsigned indent)
/*
Given the universal representation of ExprRequires, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_syntax_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_parameters(universal)) {
    an_ifc_syntax_index field = get_ifc_parameters(universal);

    db_print_indent(indent);
    fprintf(f_debug, "parameters:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_requires &universal)
/*
Given the universal representation of ExprRequires, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "ExprRequires ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_simple_identifier &universal, unsigned indent)
/*
Given the universal representation of ExprSimpleIdentifier, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_name_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_simple_identifier &universal)
/*
Given the universal representation of ExprSimpleIdentifier, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "ExprSimpleIdentifier ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_sizeof_type &universal, unsigned indent)
/*
Given the universal representation of ExprSizeofType, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operand(universal)) {
    an_ifc_type_index field = get_ifc_operand(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operand:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_sizeof_type &universal)
/*
Given the universal representation of ExprSizeofType, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "ExprSizeofType ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_string &universal, unsigned indent)
/*
Given the universal representation of ExprString, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_string_index(universal)) {
    an_ifc_string_index field = get_ifc_string_index(universal);

    db_print_indent(indent);
    fprintf(f_debug, "string_index:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_string &universal)
/*
Given the universal representation of ExprString, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "ExprString ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_string_sequence &universal, unsigned indent)
/*
Given the universal representation of ExprStringSequence, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_strings(universal)) {
    an_ifc_expr_index field = get_ifc_strings(universal);

    db_print_indent(indent);
    fprintf(f_debug, "strings:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_string_sequence &universal)
/*
Given the universal representation of ExprStringSequence, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "ExprStringSequence ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_subobject_value &universal, unsigned indent)
/*
Given the universal representation of ExprSubobjectValue, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_expr_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_subobject_value &universal)
/*
Given the universal representation of ExprSubobjectValue, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "ExprSubobjectValue ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_sum_type_value &universal, unsigned indent)
/*
Given the universal representation of ExprSumTypeValue, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_discriminant(universal)) {
    an_ifc_active_member field = get_ifc_discriminant(universal);

    db_print_indent(indent);
    fprintf(f_debug, "discriminant: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_value(universal)) {
    an_ifc_expr_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_variant(universal)) {
    an_ifc_decl_index field = get_ifc_variant(universal);

    db_print_indent(indent);
    fprintf(f_debug, "variant:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_sum_type_value &universal)
/*
Given the universal representation of ExprSumTypeValue, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "ExprSumTypeValue ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_syntax_tree &universal, unsigned indent)
/*
Given the universal representation of ExprSyntaxTree, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_syntax(universal)) {
    an_ifc_syntax_index field = get_ifc_syntax(universal);

    db_print_indent(indent);
    fprintf(f_debug, "syntax:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_syntax_tree &universal)
/*
Given the universal representation of ExprSyntaxTree, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "ExprSyntaxTree ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_template_id &universal, unsigned indent)
/*
Given the universal representation of ExprTemplateId, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_arguments(universal)) {
    an_ifc_expr_index field = get_ifc_arguments(universal);

    db_print_indent(indent);
    fprintf(f_debug, "arguments:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_primary(universal)) {
    an_ifc_expr_index field = get_ifc_primary(universal);

    db_print_indent(indent);
    fprintf(f_debug, "primary:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_template_id &universal)
/*
Given the universal representation of ExprTemplateId, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "ExprTemplateId ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_template_reference &universal, unsigned indent)
/*
Given the universal representation of ExprTemplateReference, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_arguments(universal)) {
    an_ifc_expr_index field = get_ifc_arguments(universal);

    db_print_indent(indent);
    fprintf(f_debug, "arguments:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_member(universal)) {
    an_ifc_decl_index field = get_ifc_member(universal);

    db_print_indent(indent);
    fprintf(f_debug, "member:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_member_locus(universal)) {
    an_ifc_source_location field = get_ifc_member_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "member_locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_member_name(universal)) {
    an_ifc_name_index field = get_ifc_member_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "member_name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_scope(universal)) {
    an_ifc_type_index field = get_ifc_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_template_reference &universal)
/*
Given the universal representation of ExprTemplateReference, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "ExprTemplateReference ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_temporary &universal, unsigned indent)
/*
Given the universal representation of ExprTemporary, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_id(universal)) {
    an_ifc_unique_id field = get_ifc_id(universal);

    db_print_indent(indent);
    fprintf(f_debug, "id: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_temporary &universal)
/*
Given the universal representation of ExprTemporary, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "ExprTemporary ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_this &universal, unsigned indent)
/*
Given the universal representation of ExprThis, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_this &universal)
/*
Given the universal representation of ExprThis, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "ExprThis ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_tokens &universal, unsigned indent)
/*
Given the universal representation of ExprTokens, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_words(universal)) {
    an_ifc_sentence_index field = get_ifc_words(universal);

    db_print_indent(indent);
    fprintf(f_debug, "words: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_tokens &universal)
/*
Given the universal representation of ExprTokens, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "ExprTokens ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_triad &universal, unsigned indent)
/*
Given the universal representation of ExprTriad, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_argument_0(universal)) {
    an_ifc_expr_index field = get_ifc_argument_0(universal);

    db_print_indent(indent);
    fprintf(f_debug, "argument_0:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_argument_1(universal)) {
    an_ifc_expr_index field = get_ifc_argument_1(universal);

    db_print_indent(indent);
    fprintf(f_debug, "argument_1:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_argument_2(universal)) {
    an_ifc_expr_index field = get_ifc_argument_2(universal);

    db_print_indent(indent);
    fprintf(f_debug, "argument_2:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_assoc(universal)) {
    an_ifc_triadic_operator_sort field = get_ifc_assoc(universal);

    db_print_indent(indent);
    fprintf(f_debug, "assoc: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_impl(universal)) {
    an_ifc_decl_index field = get_ifc_impl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "impl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_triad &universal)
/*
Given the universal representation of ExprTriad, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "ExprTriad ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_tuple &universal, unsigned indent)
/*
Given the universal representation of ExprTuple, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_tuple &universal)
/*
Given the universal representation of ExprTuple, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "ExprTuple ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_type &universal, unsigned indent)
/*
Given the universal representation of ExprType, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_denotation(universal)) {
    an_ifc_type_index field = get_ifc_denotation(universal);

    db_print_indent(indent);
    fprintf(f_debug, "denotation:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_type &universal)
/*
Given the universal representation of ExprType, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "ExprType ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_type_trait_intrinsic &universal,
             unsigned                               indent)
/*
Given the universal representation of ExprTypeTraitIntrinsic, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_arguments(universal)) {
    an_ifc_type_index field = get_ifc_arguments(universal);

    db_print_indent(indent);
    fprintf(f_debug, "arguments:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_intrinsic(universal)) {
    an_ifc_operator_category field = get_ifc_intrinsic(universal);

    db_print_indent(indent);
    fprintf(f_debug, "intrinsic:\n");
    db_print_indent(indent);
    fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
    ++indent;
    switch (field.sort) {
      case ifc_os_dyadic_operator:
        { an_ifc_dyadic_operator_sort &do_ref = field.variant.dyadic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "dyadic_operator: %s\n", str_for(do_ref));
        }
        break;
      case ifc_os_monadic_operator:
        { an_ifc_monadic_operator_sort &mo_ref =
                                                field.variant.monadic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "monadic_operator: %s\n", str_for(mo_ref));
        }
        break;
      case ifc_os_niladic_operator:
        { an_ifc_niladic_operator_sort &no_ref =
                                                field.variant.niladic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "niladic_operator: %s\n", str_for(no_ref));
        }
        break;
      case ifc_os_storage_instruction_operator:
        { an_ifc_storage_instruction_operator_sort &sio_ref =
                                    field.variant.storage_instruction_operator;

          db_print_indent(indent);
          fprintf(f_debug, "storage_instruction_operator: %s\n",
                  str_for(sio_ref));
        }
        break;
      case ifc_os_triadic_operator:
        { an_ifc_triadic_operator_sort &to_ref =
                                                field.variant.triadic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "triadic_operator: %s\n", str_for(to_ref));
        }
        break;
      case ifc_os_variadic_operator:
        { an_ifc_variadic_operator_sort &vo_ref =
                                               field.variant.variadic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "variadic_operator: %s\n", str_for(vo_ref));
        }
        break;
      default_is_unexpected();
    }  /* switch */
    --indent;
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_type_trait_intrinsic &universal)
/*
Given the universal representation of ExprTypeTraitIntrinsic, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "ExprTypeTraitIntrinsic ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_typeid &universal, unsigned indent)
/*
Given the universal representation of ExprTypeid, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operand(universal)) {
    an_ifc_type_index field = get_ifc_operand(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operand:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_typeid &universal)
/*
Given the universal representation of ExprTypeid, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "ExprTypeid ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_unary_fold &universal, unsigned indent)
/*
Given the universal representation of ExprUnaryFold, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_associativity(universal)) {
    an_ifc_associativity field = get_ifc_associativity(universal);

    db_print_indent(indent);
    fprintf(f_debug, "associativity: %llu\n",
            (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_expr(universal)) {
    an_ifc_expr_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operation(universal)) {
    an_ifc_dyadic_operator_sort field = get_ifc_operation(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operation: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_unary_fold &universal)
/*
Given the universal representation of ExprUnaryFold, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "ExprUnaryFold ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_unqualified_id &universal, unsigned indent)
/*
Given the universal representation of ExprUnqualifiedId, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_name_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_resolution(universal)) {
    an_ifc_expr_index field = get_ifc_resolution(universal);

    db_print_indent(indent);
    fprintf(f_debug, "resolution:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_template_keyword(universal)) {
    an_ifc_source_location field = get_ifc_template_keyword(universal);

    db_print_indent(indent);
    fprintf(f_debug, "template_keyword:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_unqualified_id &universal)
/*
Given the universal representation of ExprUnqualifiedId, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "ExprUnqualifiedId ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_unresolved_id &universal, unsigned indent)
/*
Given the universal representation of ExprUnresolvedId, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_name_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_unresolved_id &universal)
/*
Given the universal representation of ExprUnresolvedId, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "ExprUnresolvedId ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_expr_virtual_function_conversion &universal,
             unsigned                                      indent)
/*
Given the universal representation of ExprVirtualFunctionConversion, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_function(universal)) {
    an_ifc_decl_index field = get_ifc_function(universal);

    db_print_indent(indent);
    fprintf(f_debug, "function:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_expr_virtual_function_conversion &universal)
/*
Given the universal representation of ExprVirtualFunctionConversion, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "======================== ");
  fprintf(f_debug, "ExprVirtualFunctionConversion ");
  fprintf(f_debug, "=========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_catenate &universal, unsigned indent)
/*
Given the universal representation of FormCatenate, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_first(universal)) {
    an_ifc_form_index field = get_ifc_first(universal);

    db_print_indent(indent);
    fprintf(f_debug, "first:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_second(universal)) {
    an_ifc_form_index field = get_ifc_second(universal);

    db_print_indent(indent);
    fprintf(f_debug, "second:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_catenate &universal)
/*
Given the universal representation of FormCatenate, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "FormCatenate ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_character &universal, unsigned indent)
/*
Given the universal representation of FormCharacter, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_spelling(universal)) {
    an_ifc_text_offset field = get_ifc_spelling(universal);

    db_print_indent(indent);
    fprintf(f_debug, "spelling: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_character &universal)
/*
Given the universal representation of FormCharacter, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "FormCharacter ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_header &universal, unsigned indent)
/*
Given the universal representation of FormHeader, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_spelling(universal)) {
    an_ifc_text_offset field = get_ifc_spelling(universal);

    db_print_indent(indent);
    fprintf(f_debug, "spelling: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_header &universal)
/*
Given the universal representation of FormHeader, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "FormHeader ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_identifier &universal, unsigned indent)
/*
Given the universal representation of FormIdentifier, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_spelling(universal)) {
    an_ifc_text_offset field = get_ifc_spelling(universal);

    db_print_indent(indent);
    fprintf(f_debug, "spelling: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_identifier &universal)
/*
Given the universal representation of FormIdentifier, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "FormIdentifier ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_junk &universal, unsigned indent)
/*
Given the universal representation of FormJunk, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_spelling(universal)) {
    an_ifc_text_offset field = get_ifc_spelling(universal);

    db_print_indent(indent);
    fprintf(f_debug, "spelling: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_junk &universal)
/*
Given the universal representation of FormJunk, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "FormJunk ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_keyword &universal, unsigned indent)
/*
Given the universal representation of FormKeyword, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_spelling(universal)) {
    an_ifc_text_offset field = get_ifc_spelling(universal);

    db_print_indent(indent);
    fprintf(f_debug, "spelling: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_keyword &universal)
/*
Given the universal representation of FormKeyword, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "FormKeyword ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_number &universal, unsigned indent)
/*
Given the universal representation of FormNumber, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_spelling(universal)) {
    an_ifc_text_offset field = get_ifc_spelling(universal);

    db_print_indent(indent);
    fprintf(f_debug, "spelling: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_number &universal)
/*
Given the universal representation of FormNumber, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "FormNumber ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_operator &universal, unsigned indent)
/*
Given the universal representation of FormOperator, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_op(universal)) {
    an_ifc_form_operator_sort field = get_ifc_op(universal);

    db_print_indent(indent);
    fprintf(f_debug, "op: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_spelling(universal)) {
    an_ifc_text_offset field = get_ifc_spelling(universal);

    db_print_indent(indent);
    fprintf(f_debug, "spelling: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_operator &universal)
/*
Given the universal representation of FormOperator, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "FormOperator ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_parameter &universal, unsigned indent)
/*
Given the universal representation of FormParameter, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_spelling(universal)) {
    an_ifc_text_offset field = get_ifc_spelling(universal);

    db_print_indent(indent);
    fprintf(f_debug, "spelling: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_parameter &universal)
/*
Given the universal representation of FormParameter, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "FormParameter ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_parenthesized &universal, unsigned indent)
/*
Given the universal representation of FormParenthesized, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operand(universal)) {
    an_ifc_form_index field = get_ifc_operand(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operand:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_parenthesized &universal)
/*
Given the universal representation of FormParenthesized, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "FormParenthesized ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_pragma &universal, unsigned indent)
/*
Given the universal representation of FormPragma, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operand(universal)) {
    an_ifc_form_index field = get_ifc_operand(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operand:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_pragma &universal)
/*
Given the universal representation of FormPragma, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "FormPragma ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_spec &universal, unsigned indent)
/*
Given the universal representation of FormSpec, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_arguments(universal)) {
    an_ifc_expr_index field = get_ifc_arguments(universal);

    db_print_indent(indent);
    fprintf(f_debug, "arguments:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_primary_template(universal)) {
    an_ifc_decl_index field = get_ifc_primary_template(universal);

    db_print_indent(indent);
    fprintf(f_debug, "primary_template:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_spec &universal)
/*
Given the universal representation of FormSpec, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "FormSpec ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_string &universal, unsigned indent)
/*
Given the universal representation of FormString, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_spelling(universal)) {
    an_ifc_text_offset field = get_ifc_spelling(universal);

    db_print_indent(indent);
    fprintf(f_debug, "spelling: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_string &universal)
/*
Given the universal representation of FormString, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "FormString ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_stringize &universal, unsigned indent)
/*
Given the universal representation of FormStringize, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operand(universal)) {
    an_ifc_form_index field = get_ifc_operand(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operand:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_stringize &universal)
/*
Given the universal representation of FormStringize, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "FormStringize ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_tuple &universal, unsigned indent)
/*
Given the universal representation of FormTuple, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_tuple &universal)
/*
Given the universal representation of FormTuple, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "FormTuple ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_form_whitespace &universal, unsigned indent)
/*
Given the universal representation of FormWhitespace, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_form_whitespace &universal)
/*
Given the universal representation of FormWhitespace, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "FormWhitespace ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_heap_attr &universal, unsigned indent)
/*
Given the universal representation of HeapAttr, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_attr_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_heap_attr &universal)
/*
Given the universal representation of HeapAttr, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "HeapAttr ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_heap_chart &universal, unsigned indent)
/*
Given the universal representation of HeapChart, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_chart_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_heap_chart &universal)
/*
Given the universal representation of HeapChart, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "HeapChart ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_heap_decl &universal, unsigned indent)
/*
Given the universal representation of HeapDecl, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_decl_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_heap_decl &universal)
/*
Given the universal representation of HeapDecl, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "HeapDecl ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_heap_expr &universal, unsigned indent)
/*
Given the universal representation of HeapExpr, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_expr_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_heap_expr &universal)
/*
Given the universal representation of HeapExpr, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "HeapExpr ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_heap_form &universal, unsigned indent)
/*
Given the universal representation of HeapForm, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_form_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_heap_form &universal)
/*
Given the universal representation of HeapForm, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "HeapForm ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_heap_pp_form &universal, unsigned indent)
/*
Given the universal representation of HeapPPForm, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_form_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_heap_pp_form &universal)
/*
Given the universal representation of HeapPPForm, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "HeapPPForm ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_heap_stmt &universal, unsigned indent)
/*
Given the universal representation of HeapStmt, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_stmt_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_heap_stmt &universal)
/*
Given the universal representation of HeapStmt, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "HeapStmt ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_heap_string &universal, unsigned indent)
/*
Given the universal representation of HeapString, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_text_offset field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_heap_string &universal)
/*
Given the universal representation of HeapString, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "HeapString ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_heap_syntax &universal, unsigned indent)
/*
Given the universal representation of HeapSyntax, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_syntax_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_heap_syntax &universal)
/*
Given the universal representation of HeapSyntax, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "HeapSyntax ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_heap_type &universal, unsigned indent)
/*
Given the universal representation of HeapType, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_type_index field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_heap_type &universal)
/*
Given the universal representation of HeapType, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "HeapType ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_macro_function_like &universal, unsigned indent)
/*
Given the universal representation of MacroFunctionLike, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_arity_variadic(universal)) {
    db_print_indent(indent);
    fprintf(f_debug, "arity_variadic: UNIMPLEMENTED\n");
  }  /* if */
  if (has_ifc_body(universal)) {
    an_ifc_form_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_parameters(universal)) {
    an_ifc_form_index field = get_ifc_parameters(universal);

    db_print_indent(indent);
    fprintf(f_debug, "parameters:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_macro_function_like &universal)
/*
Given the universal representation of MacroFunctionLike, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "MacroFunctionLike ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_macro_object_like &universal, unsigned indent)
/*
Given the universal representation of MacroObjectLike, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_form_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_macro_object_like &universal)
/*
Given the universal representation of MacroObjectLike, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "MacroObjectLike ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_meta_tool_invocation &universal, unsigned indent)
/*
Given the universal representation of MetaToolInvocation, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_args(universal)) {
    an_ifc_sequence field = get_ifc_args(universal);

    db_print_indent(indent);
    fprintf(f_debug, "args:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_cmd(universal)) {
    an_ifc_text_offset field = get_ifc_cmd(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cmd: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_meta_tool_invocation &universal)
/*
Given the universal representation of MetaToolInvocation, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "MetaToolInvocation ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_meta_tool_maker &universal, unsigned indent)
/*
Given the universal representation of MetaToolMaker, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_value(universal)) {
    an_ifc_text_offset field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_meta_tool_maker &universal)
/*
Given the universal representation of MetaToolMaker, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "MetaToolMaker ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_module_export_reference &universal, unsigned indent)
/*
Given the universal representation of ModuleExportReference, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_reference(universal)) {
    an_ifc_module_reference field = get_ifc_reference(universal);

    db_print_indent(indent);
    fprintf(f_debug, "reference:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_module_export_reference &universal)
/*
Given the universal representation of ModuleExportReference, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "ModuleExportReference ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_module_import_reference &universal, unsigned indent)
/*
Given the universal representation of ModuleImportReference, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_reference(universal)) {
    an_ifc_module_reference field = get_ifc_reference(universal);

    db_print_indent(indent);
    fprintf(f_debug, "reference:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_module_import_reference &universal)
/*
Given the universal representation of ModuleImportReference, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "ModuleImportReference ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_name_conversion &universal, unsigned indent)
/*
Given the universal representation of NameConversion, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_encoded(universal)) {
    an_ifc_text_offset field = get_ifc_encoded(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_target(universal)) {
    an_ifc_type_index field = get_ifc_target(universal);

    db_print_indent(indent);
    fprintf(f_debug, "target:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_name_conversion &universal)
/*
Given the universal representation of NameConversion, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "NameConversion ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_name_guide &universal, unsigned indent)
/*
Given the universal representation of NameGuide, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_primary_template(universal)) {
    an_ifc_decl_index field = get_ifc_primary_template(universal);

    db_print_indent(indent);
    fprintf(f_debug, "primary_template:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_name_guide &universal)
/*
Given the universal representation of NameGuide, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "NameGuide ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_name_literal &universal, unsigned indent)
/*
Given the universal representation of NameLiteral, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_encoded(universal)) {
    an_ifc_text_offset field = get_ifc_encoded(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_name_literal &universal)
/*
Given the universal representation of NameLiteral, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "NameLiteral ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_name_operator &universal, unsigned indent)
/*
Given the universal representation of NameOperator, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_encoded(universal)) {
    an_ifc_text_offset field = get_ifc_encoded(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_operator(universal)) {
    an_ifc_operator_category field = get_ifc_operator(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operator:\n");
    db_print_indent(indent);
    fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
    ++indent;
    switch (field.sort) {
      case ifc_os_dyadic_operator:
        { an_ifc_dyadic_operator_sort &do_ref = field.variant.dyadic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "dyadic_operator: %s\n", str_for(do_ref));
        }
        break;
      case ifc_os_monadic_operator:
        { an_ifc_monadic_operator_sort &mo_ref =
                                                field.variant.monadic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "monadic_operator: %s\n", str_for(mo_ref));
        }
        break;
      case ifc_os_niladic_operator:
        { an_ifc_niladic_operator_sort &no_ref =
                                                field.variant.niladic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "niladic_operator: %s\n", str_for(no_ref));
        }
        break;
      case ifc_os_storage_instruction_operator:
        { an_ifc_storage_instruction_operator_sort &sio_ref =
                                    field.variant.storage_instruction_operator;

          db_print_indent(indent);
          fprintf(f_debug, "storage_instruction_operator: %s\n",
                  str_for(sio_ref));
        }
        break;
      case ifc_os_triadic_operator:
        { an_ifc_triadic_operator_sort &to_ref =
                                                field.variant.triadic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "triadic_operator: %s\n", str_for(to_ref));
        }
        break;
      case ifc_os_variadic_operator:
        { an_ifc_variadic_operator_sort &vo_ref =
                                               field.variant.variadic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "variadic_operator: %s\n", str_for(vo_ref));
        }
        break;
      default_is_unexpected();
    }  /* switch */
    --indent;
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_name_operator &universal)
/*
Given the universal representation of NameOperator, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "NameOperator ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_name_source_file &universal, unsigned indent)
/*
Given the universal representation of NameSourceFile, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_guard(universal)) {
    an_ifc_text_offset field = get_ifc_guard(universal);

    db_print_indent(indent);
    fprintf(f_debug, "guard: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_path(universal)) {
    an_ifc_text_offset field = get_ifc_path(universal);

    db_print_indent(indent);
    fprintf(f_debug, "path: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_name_source_file &universal)
/*
Given the universal representation of NameSourceFile, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "NameSourceFile ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_name_specialization &universal, unsigned indent)
/*
Given the universal representation of NameSpecialization, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_arguments(universal)) {
    an_ifc_expr_index field = get_ifc_arguments(universal);

    db_print_indent(indent);
    fprintf(f_debug, "arguments:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_primary(universal)) {
    an_ifc_name_index field = get_ifc_primary(universal);

    db_print_indent(indent);
    fprintf(f_debug, "primary:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_name_specialization &universal)
/*
Given the universal representation of NameSpecialization, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "NameSpecialization ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_name_template &universal, unsigned indent)
/*
Given the universal representation of NameTemplate, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_name(universal)) {
    an_ifc_name_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_name_template &universal)
/*
Given the universal representation of NameTemplate, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "NameTemplate ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_scope_descriptor &universal, unsigned indent)
/*
Given the universal representation of ScopeDescriptor, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_scope_descriptor &universal)
/*
Given the universal representation of ScopeDescriptor, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "ScopeDescriptor ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_scope_member &universal, unsigned indent)
/*
Given the universal representation of ScopeMember, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_index(universal)) {
    an_ifc_decl_index field = get_ifc_index(universal);

    db_print_indent(indent);
    fprintf(f_debug, "index:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_scope_member &universal)
/*
Given the universal representation of ScopeMember, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "ScopeMember ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_source_line &universal, unsigned indent)
/*
Given the universal representation of SourceLine, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_file(universal)) {
    an_ifc_name_index field = get_ifc_file(universal);

    db_print_indent(indent);
    fprintf(f_debug, "file:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_line(universal)) {
    an_ifc_line_number field = get_ifc_line(universal);

    db_print_indent(indent);
    fprintf(f_debug, "line: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_source_line &universal)
/*
Given the universal representation of SourceLine, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "SourceLine ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_source_sentence &universal, unsigned indent)
/*
Given the universal representation of SourceSentence, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_source_sentence &universal)
/*
Given the universal representation of SourceSentence, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "SourceSentence ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_source_word &universal, unsigned indent)
/*
Given the universal representation of SourceWord, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_category(universal)) {
    an_ifc_word_category field = get_ifc_category(universal);

    db_print_indent(indent);
    fprintf(f_debug, "category:\n");
    db_print_indent(indent);
    fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
    ++indent;
    switch (field.sort) {
      case ifc_ws_source_directive:
        { an_ifc_source_directive_sort &sd_ref =
                                                field.variant.source_directive;

          db_print_indent(indent);
          fprintf(f_debug, "source_directive: %s\n", str_for(sd_ref));
        }
        break;
      case ifc_ws_source_identifier:
        { an_ifc_source_identifier_category &si_ref =
                                               field.variant.source_identifier;

          db_print_indent(indent);
          fprintf(f_debug, "source_identifier:\n");
          db_print_indent(indent);
          fprintf(f_debug, "  sort: %s\n", str_for(si_ref.sort));
          ++indent;
          switch (si_ref.sort) {
            case ifc_sis_msvc:
              { an_ifc_source_unknown_identifier &m_ref = si_ref.variant.msvc;

                db_print_indent(indent);
                fprintf(f_debug, "msvc: %llu\n",
                        (unsigned long long)m_ref.value);
              }
              break;
            case ifc_sis_msvc_builtin_huge_val:
              { an_ifc_source_unknown_identifier &mbhv_ref =
                                          si_ref.variant.msvc_builtin_huge_val;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_builtin_huge_val: %llu\n",
                        (unsigned long long)mbhv_ref.value);
              }
              break;
            case ifc_sis_msvc_builtin_huge_valf:
              { an_ifc_source_unknown_identifier &mbhv_ref =
                                         si_ref.variant.msvc_builtin_huge_valf;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_builtin_huge_valf: %llu\n",
                        (unsigned long long)mbhv_ref.value);
              }
              break;
            case ifc_sis_msvc_builtin_nan:
              { an_ifc_source_unknown_identifier &mbn_ref =
                                               si_ref.variant.msvc_builtin_nan;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_builtin_nan: %llu\n",
                        (unsigned long long)mbn_ref.value);
              }
              break;
            case ifc_sis_msvc_builtin_nanf:
              { an_ifc_source_unknown_identifier &mbn_ref =
                                              si_ref.variant.msvc_builtin_nanf;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_builtin_nanf: %llu\n",
                        (unsigned long long)mbn_ref.value);
              }
              break;
            case ifc_sis_msvc_builtin_nans:
              { an_ifc_source_unknown_identifier &mbn_ref =
                                              si_ref.variant.msvc_builtin_nans;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_builtin_nans: %llu\n",
                        (unsigned long long)mbn_ref.value);
              }
              break;
            case ifc_sis_msvc_builtin_nansf:
              { an_ifc_source_unknown_identifier &mbn_ref =
                                             si_ref.variant.msvc_builtin_nansf;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_builtin_nansf: %llu\n",
                        (unsigned long long)mbn_ref.value);
              }
              break;
            case ifc_sis_plain:
              { an_ifc_text_offset &p_ref = si_ref.variant.plain;

                db_print_indent(indent);
                fprintf(f_debug, "plain: %llu\n",
                        (unsigned long long)p_ref.value);
              }
              break;
            default_is_unexpected();
          }  /* switch */
          --indent;
        }
        break;
      case ifc_ws_source_keyword:
        { an_ifc_source_keyword_sort &sk_ref = field.variant.source_keyword;

          db_print_indent(indent);
          fprintf(f_debug, "source_keyword: %s\n", str_for(sk_ref));
        }
        break;
      case ifc_ws_source_literal:
        { an_ifc_source_literal_category &sl_ref =
                                                  field.variant.source_literal;

          db_print_indent(indent);
          fprintf(f_debug, "source_literal:\n");
          db_print_indent(indent);
          fprintf(f_debug, "  sort: %s\n", str_for(sl_ref.sort));
          ++indent;
          switch (sl_ref.sort) {
            case ifc_sls_defined_string:
              { an_ifc_string_index &ds_ref = sl_ref.variant.defined_string;

                db_print_indent(indent);
                fprintf(f_debug, "defined_string:");
                if (is_null_index(ds_ref)) {
                  fprintf(f_debug, " NULL\n");
                } else {
                  fprintf(f_debug, "\n");
                  db_print_indent(indent);
                  fprintf(f_debug, "  sort: %s\n", str_for(ds_ref.sort));
                  db_print_indent(indent);
                  fprintf(f_debug, "  value: %llu\n",
                          (unsigned long long)ds_ref.value);
                }  /* if */
              }
              break;
            case ifc_sls_msvc:
              { an_ifc_source_unknown_literal &m_ref = sl_ref.variant.msvc;

                db_print_indent(indent);
                fprintf(f_debug, "msvc: %llu\n",
                        (unsigned long long)m_ref.value);
              }
              break;
            case ifc_sls_msvc_binding:
              { an_ifc_expr_index &mb_ref = sl_ref.variant.msvc_binding;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_binding:");
                if (is_null_index(mb_ref)) {
                  fprintf(f_debug, " NULL\n");
                } else {
                  fprintf(f_debug, "\n");
                  db_print_indent(indent);
                  fprintf(f_debug, "  sort: %s\n", str_for(mb_ref.sort));
                  db_print_indent(indent);
                  fprintf(f_debug, "  value: %llu\n",
                          (unsigned long long)mb_ref.value);
                }  /* if */
              }
              break;
            case ifc_sls_msvc_cast_target_type:
              { an_ifc_type_index &mctt_ref =
                                          sl_ref.variant.msvc_cast_target_type;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_cast_target_type:");
                if (is_null_index(mctt_ref)) {
                  fprintf(f_debug, " NULL\n");
                } else {
                  fprintf(f_debug, "\n");
                  db_print_indent(indent);
                  fprintf(f_debug, "  sort: %s\n", str_for(mctt_ref.sort));
                  db_print_indent(indent);
                  fprintf(f_debug, "  value: %llu\n",
                          (unsigned long long)mctt_ref.value);
                }  /* if */
              }
              break;
            case ifc_sls_msvc_defined_constant:
              { an_ifc_expr_index &mdc_ref =
                                          sl_ref.variant.msvc_defined_constant;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_defined_constant:");
                if (is_null_index(mdc_ref)) {
                  fprintf(f_debug, " NULL\n");
                } else {
                  fprintf(f_debug, "\n");
                  db_print_indent(indent);
                  fprintf(f_debug, "  sort: %s\n", str_for(mdc_ref.sort));
                  db_print_indent(indent);
                  fprintf(f_debug, "  value: %llu\n",
                          (unsigned long long)mdc_ref.value);
                }  /* if */
              }
              break;
            case ifc_sls_msvc_function_name_macro:
              { an_ifc_text_offset &mfnm_ref =
                                       sl_ref.variant.msvc_function_name_macro;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_function_name_macro: %llu\n",
                        (unsigned long long)mfnm_ref.value);
              }
              break;
            case ifc_sls_msvc_resolved_type:
              { an_ifc_type_index &mrt_ref = sl_ref.variant.msvc_resolved_type;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_resolved_type:");
                if (is_null_index(mrt_ref)) {
                  fprintf(f_debug, " NULL\n");
                } else {
                  fprintf(f_debug, "\n");
                  db_print_indent(indent);
                  fprintf(f_debug, "  sort: %s\n", str_for(mrt_ref.sort));
                  db_print_indent(indent);
                  fprintf(f_debug, "  value: %llu\n",
                          (unsigned long long)mrt_ref.value);
                }  /* if */
              }
              break;
            case ifc_sls_msvc_string_prefix_macro:
              { an_ifc_text_offset &mspm_ref =
                                       sl_ref.variant.msvc_string_prefix_macro;

                db_print_indent(indent);
                fprintf(f_debug, "msvc_string_prefix_macro: %llu\n",
                        (unsigned long long)mspm_ref.value);
              }
              break;
            case ifc_sls_scalar:
              { an_ifc_expr_index &s_ref = sl_ref.variant.scalar;

                db_print_indent(indent);
                fprintf(f_debug, "scalar:");
                if (is_null_index(s_ref)) {
                  fprintf(f_debug, " NULL\n");
                } else {
                  fprintf(f_debug, "\n");
                  db_print_indent(indent);
                  fprintf(f_debug, "  sort: %s\n", str_for(s_ref.sort));
                  db_print_indent(indent);
                  fprintf(f_debug, "  value: %llu\n",
                          (unsigned long long)s_ref.value);
                }  /* if */
              }
              break;
            case ifc_sls_string:
              { an_ifc_string_index &s_ref = sl_ref.variant.string;

                db_print_indent(indent);
                fprintf(f_debug, "string:");
                if (is_null_index(s_ref)) {
                  fprintf(f_debug, " NULL\n");
                } else {
                  fprintf(f_debug, "\n");
                  db_print_indent(indent);
                  fprintf(f_debug, "  sort: %s\n", str_for(s_ref.sort));
                  db_print_indent(indent);
                  fprintf(f_debug, "  value: %llu\n",
                          (unsigned long long)s_ref.value);
                }  /* if */
              }
              break;
            case ifc_sls_unknown:
              { an_ifc_source_unknown_literal &u_ref = sl_ref.variant.unknown;

                db_print_indent(indent);
                fprintf(f_debug, "unknown: %llu\n",
                        (unsigned long long)u_ref.value);
              }
              break;
            default_is_unexpected();
          }  /* switch */
          --indent;
        }
        break;
      case ifc_ws_source_operator:
        { an_ifc_source_operator_sort &so_ref = field.variant.source_operator;

          db_print_indent(indent);
          fprintf(f_debug, "source_operator: %s\n", str_for(so_ref));
        }
        break;
      case ifc_ws_source_punctuator:
        { an_ifc_source_punctuator_sort &sp_ref =
                                               field.variant.source_punctuator;

          db_print_indent(indent);
          fprintf(f_debug, "source_punctuator: %s\n", str_for(sp_ref));
        }
        break;
      case ifc_ws_unknown:
        { an_ifc_source_unknown_word &u_ref = field.variant.unknown;

          db_print_indent(indent);
          fprintf(f_debug, "unknown: %llu\n",
                  (unsigned long long)u_ref.value);
        }
        break;
      default_is_unexpected();
    }  /* switch */
    --indent;
  }  /* if */
  if (has_ifc_index(universal)) {
    an_ifc_index field = get_ifc_index(universal);

    db_print_indent(indent);
    fprintf(f_debug, "index: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_sort(universal)) {
    an_ifc_word_sort field = get_ifc_sort(universal);

    db_print_indent(indent);
    fprintf(f_debug, "sort: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_value(universal)) {
    an_ifc_u16 field = get_ifc_value(universal);

    db_print_indent(indent);
    fprintf(f_debug, "value: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_source_word &universal)
/*
Given the universal representation of SourceWord, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "SourceWord ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_block &universal, unsigned indent)
/*
Given the universal representation of StmtBlock, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_block &universal)
/*
Given the universal representation of StmtBlock, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "StmtBlock ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_break &universal, unsigned indent)
/*
Given the universal representation of StmtBreak, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_break &universal)
/*
Given the universal representation of StmtBreak, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "StmtBreak ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_case &universal, unsigned indent)
/*
Given the universal representation of StmtCase, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_expr(universal)) {
    an_ifc_expr_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_case &universal)
/*
Given the universal representation of StmtCase, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "StmtCase ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_continue &universal, unsigned indent)
/*
Given the universal representation of StmtContinue, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_continue &universal)
/*
Given the universal representation of StmtContinue, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "StmtContinue ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_decl &universal, unsigned indent)
/*
Given the universal representation of StmtDecl, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_decl &universal)
/*
Given the universal representation of StmtDecl, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "StmtDecl ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_default &universal, unsigned indent)
/*
Given the universal representation of StmtDefault, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_default &universal)
/*
Given the universal representation of StmtDefault, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "StmtDefault ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_do_while &universal, unsigned indent)
/*
Given the universal representation of StmtDoWhile, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_stmt_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_condition(universal)) {
    an_ifc_stmt_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_do_while &universal)
/*
Given the universal representation of StmtDoWhile, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "StmtDoWhile ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_empty &universal, unsigned indent)
/*
Given the universal representation of StmtEmpty, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_empty &universal)
/*
Given the universal representation of StmtEmpty, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "StmtEmpty ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_expansion &universal, unsigned indent)
/*
Given the universal representation of StmtExpansion, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operand(universal)) {
    an_ifc_stmt_index field = get_ifc_operand(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operand:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_expansion &universal)
/*
Given the universal representation of StmtExpansion, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "StmtExpansion ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_expression &universal, unsigned indent)
/*
Given the universal representation of StmtExpression, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_expr(universal)) {
    an_ifc_expr_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_expression &universal)
/*
Given the universal representation of StmtExpression, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "StmtExpression ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_for &universal, unsigned indent)
/*
Given the universal representation of StmtFor, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_stmt_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_condition(universal)) {
    an_ifc_stmt_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_continuation(universal)) {
    an_ifc_stmt_index field = get_ifc_continuation(universal);

    db_print_indent(indent);
    fprintf(f_debug, "continuation:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initialization(universal)) {
    an_ifc_stmt_index field = get_ifc_initialization(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initialization:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_for &universal)
/*
Given the universal representation of StmtFor, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "StmtFor ");
  fprintf(f_debug, "====================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_goto &universal, unsigned indent)
/*
Given the universal representation of StmtGoto, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_target(universal)) {
    an_ifc_expr_index field = get_ifc_target(universal);

    db_print_indent(indent);
    fprintf(f_debug, "target:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_goto &universal)
/*
Given the universal representation of StmtGoto, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "StmtGoto ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_handler &universal, unsigned indent)
/*
Given the universal representation of StmtHandler, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_stmt_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_exception(universal)) {
    an_ifc_decl_index field = get_ifc_exception(universal);

    db_print_indent(indent);
    fprintf(f_debug, "exception:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_handler &universal)
/*
Given the universal representation of StmtHandler, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "StmtHandler ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_if &universal, unsigned indent)
/*
Given the universal representation of StmtIf, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_alternative(universal)) {
    an_ifc_stmt_index field = get_ifc_alternative(universal);

    db_print_indent(indent);
    fprintf(f_debug, "alternative:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_condition(universal)) {
    an_ifc_stmt_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_consequence(universal)) {
    an_ifc_stmt_index field = get_ifc_consequence(universal);

    db_print_indent(indent);
    fprintf(f_debug, "consequence:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initialization(universal)) {
    an_ifc_stmt_index field = get_ifc_initialization(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initialization:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_if &universal)
/*
Given the universal representation of StmtIf, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "==================================== ");
  fprintf(f_debug, "StmtIf ");
  fprintf(f_debug, "====================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_labeled &universal, unsigned indent)
/*
Given the universal representation of StmtLabeled, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_label(universal)) {
    an_ifc_expr_index field = get_ifc_label(universal);

    db_print_indent(indent);
    fprintf(f_debug, "label:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_stmt(universal)) {
    an_ifc_stmt_index field = get_ifc_stmt(universal);

    db_print_indent(indent);
    fprintf(f_debug, "stmt:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_labeled &universal)
/*
Given the universal representation of StmtLabeled, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "StmtLabeled ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_return &universal, unsigned indent)
/*
Given the universal representation of StmtReturn, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_expr(universal)) {
    an_ifc_expr_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_function_type(universal)) {
    an_ifc_type_index field = get_ifc_function_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "function_type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_return &universal)
/*
Given the universal representation of StmtReturn, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "StmtReturn ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_switch &universal, unsigned indent)
/*
Given the universal representation of StmtSwitch, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_stmt_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_condition(universal)) {
    an_ifc_expr_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initialization(universal)) {
    an_ifc_stmt_index field = get_ifc_initialization(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initialization:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_switch &universal)
/*
Given the universal representation of StmtSwitch, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "StmtSwitch ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_try &universal, unsigned indent)
/*
Given the universal representation of StmtTry, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_handlers(universal)) {
    an_ifc_stmt_index field = get_ifc_handlers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "handlers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_try &universal)
/*
Given the universal representation of StmtTry, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "StmtTry ");
  fprintf(f_debug, "====================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_tuple &universal, unsigned indent)
/*
Given the universal representation of StmtTuple, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_tuple &universal)
/*
Given the universal representation of StmtTuple, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "StmtTuple ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_variable_decl &universal, unsigned indent)
/*
Given the universal representation of StmtVariableDecl, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_variable_decl &universal)
/*
Given the universal representation of StmtVariableDecl, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "StmtVariableDecl ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_stmt_while &universal, unsigned indent)
/*
Given the universal representation of StmtWhile, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_stmt_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_condition(universal)) {
    an_ifc_stmt_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_stmt_while &universal)
/*
Given the universal representation of StmtWhile, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "StmtWhile ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_access_specifier &universal, unsigned indent)
/*
Given the universal representation of SyntaxAccessSpecifier, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_keyword_syntax field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_designator(universal)) {
    an_ifc_expr_index field = get_ifc_designator(universal);

    db_print_indent(indent);
    fprintf(f_debug, "designator:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_virtual_kw(universal)) {
    an_ifc_source_location field = get_ifc_virtual_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "virtual_kw:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_virtual_kw2(universal)) {
    an_ifc_source_location field = get_ifc_virtual_kw2(universal);

    db_print_indent(indent);
    fprintf(f_debug, "virtual_kw2:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_access_specifier &universal)
/*
Given the universal representation of SyntaxAccessSpecifier, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxAccessSpecifier ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_alias_declaration &universal, unsigned indent)
/*
Given the universal representation of SyntaxAliasDeclaration, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_aliasee(universal)) {
    an_ifc_syntax_index field = get_ifc_aliasee(universal);

    db_print_indent(indent);
    fprintf(f_debug, "aliasee:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_equal(universal)) {
    an_ifc_source_location field = get_ifc_equal(universal);

    db_print_indent(indent);
    fprintf(f_debug, "equal:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_expr_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_alias_declaration &universal)
/*
Given the universal representation of SyntaxAliasDeclaration, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxAliasDeclaration ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_alignas &universal, unsigned indent)
/*
Given the universal representation of SyntaxAlignas, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_left_paren(universal)) {
    an_ifc_source_location field = get_ifc_left_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operand(universal)) {
    an_ifc_syntax_index field = get_ifc_operand(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operand:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_alignas &universal)
/*
Given the universal representation of SyntaxAlignas, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "SyntaxAlignas ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_array_declarator &universal, unsigned indent)
/*
Given the universal representation of SyntaxArrayDeclarator, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_bound(universal)) {
    an_ifc_expr_index field = get_ifc_bound(universal);

    db_print_indent(indent);
    fprintf(f_debug, "bound:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_left_bracket(universal)) {
    an_ifc_source_location field = get_ifc_left_bracket(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_bracket:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_right_bracket(universal)) {
    an_ifc_source_location field = get_ifc_right_bracket(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_bracket:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_array_declarator &universal)
/*
Given the universal representation of SyntaxArrayDeclarator, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxArrayDeclarator ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_array_index &universal, unsigned indent)
/*
Given the universal representation of SyntaxArrayIndex, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_array(universal)) {
    an_ifc_expr_index field = get_ifc_array(universal);

    db_print_indent(indent);
    fprintf(f_debug, "array:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_index(universal)) {
    an_ifc_expr_index field = get_ifc_index(universal);

    db_print_indent(indent);
    fprintf(f_debug, "index:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_left_bracket(universal)) {
    an_ifc_source_location field = get_ifc_left_bracket(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_bracket:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_right_bracket(universal)) {
    an_ifc_source_location field = get_ifc_right_bracket(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_bracket:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_array_index &universal)
/*
Given the universal representation of SyntaxArrayIndex, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "SyntaxArrayIndex ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_array_or_function_declarator &universal,
             unsigned                                         indent)
/*
Given the universal representation of SyntaxArrayOrFunctionDeclarator, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_declarator(universal)) {
    an_ifc_syntax_index field = get_ifc_declarator(universal);

    db_print_indent(indent);
    fprintf(f_debug, "declarator:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_next(universal)) {
    an_ifc_syntax_index field = get_ifc_next(universal);

    db_print_indent(indent);
    fprintf(f_debug, "next:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_array_or_function_declarator &universal)
/*
Given the universal representation of SyntaxArrayOrFunctionDeclarator, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "======================= ");
  fprintf(f_debug, "SyntaxArrayOrFunctionDeclarator ");
  fprintf(f_debug, "========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_asm_statement &universal, unsigned indent)
/*
Given the universal representation of SyntaxAsmStatement, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_tokens(universal)) {
    an_ifc_sentence_index field = get_ifc_tokens(universal);

    db_print_indent(indent);
    fprintf(f_debug, "tokens: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_asm_statement &universal)
/*
Given the universal representation of SyntaxAsmStatement, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "SyntaxAsmStatement ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_attribute &universal, unsigned indent)
/*
Given the universal representation of SyntaxAttribute, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_argument_clause(universal)) {
    an_ifc_syntax_index field = get_ifc_argument_clause(universal);

    db_print_indent(indent);
    fprintf(f_debug, "argument_clause:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_colons(universal)) {
    an_ifc_source_location field = get_ifc_colons(universal);

    db_print_indent(indent);
    fprintf(f_debug, "colons:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_expander(universal)) {
    an_ifc_source_location field = get_ifc_expander(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expander:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_expr_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_scope(universal)) {
    an_ifc_expr_index field = get_ifc_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_attribute &universal)
/*
Given the universal representation of SyntaxAttribute, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "SyntaxAttribute ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_attribute_argument_clause &universal,
             unsigned                                      indent)
/*
Given the universal representation of SyntaxAttributeArgumentClause, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_left_paren(universal)) {
    an_ifc_source_location field = get_ifc_left_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_tokens(universal)) {
    an_ifc_sentence_index field = get_ifc_tokens(universal);

    db_print_indent(indent);
    fprintf(f_debug, "tokens: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_attribute_argument_clause &universal)
/*
Given the universal representation of SyntaxAttributeArgumentClause, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "======================== ");
  fprintf(f_debug, "SyntaxAttributeArgumentClause ");
  fprintf(f_debug, "=========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_attribute_specifier &universal,
             unsigned                                indent)
/*
Given the universal representation of SyntaxAttributeSpecifier, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_attributes(universal)) {
    an_ifc_syntax_index field = get_ifc_attributes(universal);

    db_print_indent(indent);
    fprintf(f_debug, "attributes:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_left_paren_1(universal)) {
    an_ifc_source_location field = get_ifc_left_paren_1(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren_1:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_left_paren_2(universal)) {
    an_ifc_source_location field = get_ifc_left_paren_2(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren_2:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_prefix(universal)) {
    an_ifc_syntax_index field = get_ifc_prefix(universal);

    db_print_indent(indent);
    fprintf(f_debug, "prefix:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_right_paren_1(universal)) {
    an_ifc_source_location field = get_ifc_right_paren_1(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren_1:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_right_paren_2(universal)) {
    an_ifc_source_location field = get_ifc_right_paren_2(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren_2:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_attribute_specifier &universal)
/*
Given the universal representation of SyntaxAttributeSpecifier, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxAttributeSpecifier ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_attribute_specifier_seq &universal,
             unsigned                                    indent)
/*
Given the universal representation of SyntaxAttributeSpecifierSeq, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_attributes(universal)) {
    an_ifc_syntax_index field = get_ifc_attributes(universal);

    db_print_indent(indent);
    fprintf(f_debug, "attributes:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_attribute_specifier_seq &universal)
/*
Given the universal representation of SyntaxAttributeSpecifierSeq, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================= ");
  fprintf(f_debug, "SyntaxAttributeSpecifierSeq ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_attribute_using_prefix &universal,
             unsigned                                   indent)
/*
Given the universal representation of SyntaxAttributeUsingPrefix, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_scope(universal)) {
    an_ifc_source_location field = get_ifc_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "scope:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_attribute_using_prefix &universal)
/*
Given the universal representation of SyntaxAttributeUsingPrefix, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxAttributeUsingPrefix ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_attributed_declaration &universal,
             unsigned                                   indent)
/*
Given the universal representation of SyntaxAttributedDeclaration, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_attributes(universal)) {
    an_ifc_syntax_index field = get_ifc_attributes(universal);

    db_print_indent(indent);
    fprintf(f_debug, "attributes:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_decl(universal)) {
    an_ifc_syntax_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_attributed_declaration &universal)
/*
Given the universal representation of SyntaxAttributedDeclaration, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================= ");
  fprintf(f_debug, "SyntaxAttributedDeclaration ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_attributed_statement &universal,
             unsigned                                 indent)
/*
Given the universal representation of SyntaxAttributedStatement, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_attributes(universal)) {
    an_ifc_syntax_index field = get_ifc_attributes(universal);

    db_print_indent(indent);
    fprintf(f_debug, "attributes:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_stmt(universal)) {
    an_ifc_syntax_index field = get_ifc_stmt(universal);

    db_print_indent(indent);
    fprintf(f_debug, "stmt:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_attributed_statement &universal)
/*
Given the universal representation of SyntaxAttributedStatement, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxAttributedStatement ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_base_specifier &universal, unsigned indent)
/*
Given the universal representation of SyntaxBaseSpecifier, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_keyword_syntax field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_colon(universal)) {
    an_ifc_source_location field = get_ifc_colon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "colon:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_base_specifier &universal)
/*
Given the universal representation of SyntaxBaseSpecifier, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxBaseSpecifier ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_base_specifier_list &universal,
             unsigned                                indent)
/*
Given the universal representation of SyntaxBaseSpecifierList, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_base_specifiers(universal)) {
    an_ifc_syntax_index field = get_ifc_base_specifiers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "base_specifiers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_colon(universal)) {
    an_ifc_source_location field = get_ifc_colon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "colon:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_base_specifier_list &universal)
/*
Given the universal representation of SyntaxBaseSpecifierList, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxBaseSpecifierList ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_binary_fold_expression &universal,
             unsigned                                   indent)
/*
Given the universal representation of SyntaxBinaryFoldExpression, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_direction(universal)) {
    an_ifc_fold_direction_sort field = get_ifc_direction(universal);

    db_print_indent(indent);
    fprintf(f_debug, "direction: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_dyad(universal)) {
    an_ifc_dyadic_operator_sort field = get_ifc_dyad(universal);

    db_print_indent(indent);
    fprintf(f_debug, "dyad: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_ellipsis(universal)) {
    an_ifc_source_location field = get_ifc_ellipsis(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ellipsis:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_glyph_loci_1(universal)) {
    an_ifc_source_location field = get_ifc_glyph_loci_1(universal);

    db_print_indent(indent);
    fprintf(f_debug, "glyph_loci_1:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_glyph_loci_2(universal)) {
    an_ifc_source_location field = get_ifc_glyph_loci_2(universal);

    db_print_indent(indent);
    fprintf(f_debug, "glyph_loci_2:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operand_1(universal)) {
    an_ifc_expr_index field = get_ifc_operand_1(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operand_1:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_operand_2(universal)) {
    an_ifc_expr_index field = get_ifc_operand_2(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operand_2:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_binary_fold_expression &universal)
/*
Given the universal representation of SyntaxBinaryFoldExpression, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxBinaryFoldExpression ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_break_statement &universal, unsigned indent)
/*
Given the universal representation of SyntaxBreakStatement, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_break(universal)) {
    an_ifc_source_location field = get_ifc_break(universal);

    db_print_indent(indent);
    fprintf(f_debug, "break:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_break_statement &universal)
/*
Given the universal representation of SyntaxBreakStatement, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxBreakStatement ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_capture_default &universal, unsigned indent)
/*
Given the universal representation of SyntaxCaptureDefault, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_by_ref(universal)) {
    an_ifc_bool field = get_ifc_by_ref(universal);

    db_print_indent(indent);
    fprintf(f_debug, "by_ref: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_capture_default &universal)
/*
Given the universal representation of SyntaxCaptureDefault, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxCaptureDefault ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_class_specifier &universal, unsigned indent)
/*
Given the universal representation of SyntaxClassSpecifier, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_bases(universal)) {
    an_ifc_syntax_index field = get_ifc_bases(universal);

    db_print_indent(indent);
    fprintf(f_debug, "bases:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_class_key(universal)) {
    an_ifc_keyword_syntax field = get_ifc_class_key(universal);

    db_print_indent(indent);
    fprintf(f_debug, "class_key:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_left_paren(universal)) {
    an_ifc_syntax_index field = get_ifc_left_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_members(universal)) {
    an_ifc_syntax_index field = get_ifc_members(universal);

    db_print_indent(indent);
    fprintf(f_debug, "members:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_expr_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_syntax_index field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_class_specifier &universal)
/*
Given the universal representation of SyntaxClassSpecifier, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxClassSpecifier ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_compound_requirement &universal,
             unsigned                                 indent)
/*
Given the universal representation of SyntaxCompoundRequirement, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_condition(universal)) {
    an_ifc_expr_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_constraint(universal)) {
    an_ifc_expr_index field = get_ifc_constraint(universal);

    db_print_indent(indent);
    fprintf(f_debug, "constraint:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_noexcept_loc(universal)) {
    an_ifc_source_location field = get_ifc_noexcept_loc(universal);

    db_print_indent(indent);
    fprintf(f_debug, "noexcept_loc:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_right_curly(universal)) {
    an_ifc_source_location field = get_ifc_right_curly(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_curly:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_compound_requirement &universal)
/*
Given the universal representation of SyntaxCompoundRequirement, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxCompoundRequirement ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_compound_statement &universal,
             unsigned                               indent)
/*
Given the universal representation of SyntaxCompoundStatement, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_left_curly(universal)) {
    an_ifc_source_location field = get_ifc_left_curly(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_curly:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_pragam(universal)) {
    an_ifc_sentence_index field = get_ifc_pragam(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragam: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_right_curly(universal)) {
    an_ifc_source_location field = get_ifc_right_curly(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_curly:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_stmts(universal)) {
    an_ifc_syntax_index field = get_ifc_stmts(universal);

    db_print_indent(indent);
    fprintf(f_debug, "stmts:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_compound_statement &universal)
/*
Given the universal representation of SyntaxCompoundStatement, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxCompoundStatement ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_concept_definition &universal,
             unsigned                               indent)
/*
Given the universal representation of SyntaxConceptDefinition, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_concept_keyword(universal)) {
    an_ifc_source_location field = get_ifc_concept_keyword(universal);

    db_print_indent(indent);
    fprintf(f_debug, "concept_keyword:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_equal(universal)) {
    an_ifc_source_location field = get_ifc_equal(universal);

    db_print_indent(indent);
    fprintf(f_debug, "equal:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_parameters(universal)) {
    an_ifc_syntax_index field = get_ifc_parameters(universal);

    db_print_indent(indent);
    fprintf(f_debug, "parameters:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_concept_definition &universal)
/*
Given the universal representation of SyntaxConceptDefinition, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxConceptDefinition ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_condition_declaration &universal,
             unsigned                                  indent)
/*
Given the universal representation of SyntaxConditionDeclaration, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_decl_specifier(universal)) {
    an_ifc_syntax_index field = get_ifc_decl_specifier(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl_specifier:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initializaerion(universal)) {
    an_ifc_syntax_index field = get_ifc_initializaerion(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializaerion:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_condition_declaration &universal)
/*
Given the universal representation of SyntaxConditionDeclaration, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxConditionDeclaration ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_continue_statement &universal,
             unsigned                               indent)
/*
Given the universal representation of SyntaxContinueStatement, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_continue(universal)) {
    an_ifc_source_location field = get_ifc_continue(universal);

    db_print_indent(indent);
    fprintf(f_debug, "continue:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_continue_statement &universal)
/*
Given the universal representation of SyntaxContinueStatement, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxContinueStatement ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_ctor_initializer &universal, unsigned indent)
/*
Given the universal representation of SyntaxCtorInitializer, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_colon(universal)) {
    an_ifc_source_location field = get_ifc_colon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "colon:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_initializers(universal)) {
    an_ifc_syntax_index field = get_ifc_initializers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_ctor_initializer &universal)
/*
Given the universal representation of SyntaxCtorInitializer, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxCtorInitializer ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_decl_specifier_seq &universal,
             unsigned                               indent)
/*
Given the universal representation of SyntaxDeclSpecifierSeq, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_declspec(universal)) {
    an_ifc_sentence_index field = get_ifc_declspec(universal);

    db_print_indent(indent);
    fprintf(f_debug, "declspec: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_explicit_kw(universal)) {
    an_ifc_syntax_index field = get_ifc_explicit_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "explicit_kw:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_qualifiers(universal)) {
    an_ifc_qualifier_bitfield field = get_ifc_qualifiers(universal);

    fprintf(f_debug, "qualifiers:\n");
    ++indent;
    if (test_bitmask<ifc_qb_const>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Const\n");
    }  /* if */
    if (test_bitmask<ifc_qb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_qb_restrict>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Restrict\n");
    }  /* if */
    if (test_bitmask<ifc_qb_volatile>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Volatile\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_storage_class(universal)) {
    db_print_indent(indent);
    fprintf(f_debug, "storage_class: UNIMPLEMENTED\n");
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type_name(universal)) {
    an_ifc_syntax_index field = get_ifc_type_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type_name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_decl_specifier_seq &universal)
/*
Given the universal representation of SyntaxDeclSpecifierSeq, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxDeclSpecifierSeq ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_declaration_statement &universal,
             unsigned                                  indent)
/*
Given the universal representation of SyntaxDeclarationStatement, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_syntax_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_declaration_statement &universal)
/*
Given the universal representation of SyntaxDeclarationStatement, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxDeclarationStatement ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_declarator &universal, unsigned indent)
/*
Given the universal representation of SyntaxDeclarator, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_array_or_function(universal)) {
    an_ifc_syntax_index field = get_ifc_array_or_function(universal);

    db_print_indent(indent);
    fprintf(f_debug, "array_or_function:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_callable(universal)) {
    an_ifc_bool field = get_ifc_callable(universal);

    db_print_indent(indent);
    fprintf(f_debug, "callable: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_convention(universal)) {
    an_ifc_calling_convention_sort field = get_ifc_convention(universal);

    db_print_indent(indent);
    fprintf(f_debug, "convention: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_ellipsis(universal)) {
    an_ifc_source_location field = get_ifc_ellipsis(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ellipsis:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_expr_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_parenthesized(universal)) {
    an_ifc_syntax_index field = get_ifc_parenthesized(universal);

    db_print_indent(indent);
    fprintf(f_debug, "parenthesized:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_pointer(universal)) {
    an_ifc_syntax_index field = get_ifc_pointer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pointer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_qualifiers(universal)) {
    an_ifc_qualifier_bitfield field = get_ifc_qualifiers(universal);

    fprintf(f_debug, "qualifiers:\n");
    ++indent;
    if (test_bitmask<ifc_qb_const>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Const\n");
    }  /* if */
    if (test_bitmask<ifc_qb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_qb_restrict>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Restrict\n");
    }  /* if */
    if (test_bitmask<ifc_qb_volatile>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Volatile\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_trailing_target(universal)) {
    an_ifc_syntax_index field = get_ifc_trailing_target(universal);

    db_print_indent(indent);
    fprintf(f_debug, "trailing_target:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_virtual_specifiers(universal)) {
    an_ifc_syntax_index field = get_ifc_virtual_specifiers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "virtual_specifiers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_declarator &universal)
/*
Given the universal representation of SyntaxDeclarator, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "SyntaxDeclarator ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_decltype_specifier &universal,
             unsigned                               indent)
/*
Given the universal representation of SyntaxDecltypeSpecifier, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_decltype_keyword(universal)) {
    an_ifc_source_location field = get_ifc_decltype_keyword(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decltype_keyword:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_expr(universal)) {
    an_ifc_expr_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_left_paren(universal)) {
    an_ifc_source_location field = get_ifc_left_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_decltype_specifier &universal)
/*
Given the universal representation of SyntaxDecltypeSpecifier, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxDecltypeSpecifier ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_do_while_statement &universal,
             unsigned                               indent)
/*
Given the universal representation of SyntaxDoWhileStatement, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_syntax_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_condition(universal)) {
    an_ifc_expr_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_do(universal)) {
    an_ifc_source_location field = get_ifc_do(universal);

    db_print_indent(indent);
    fprintf(f_debug, "do:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_while(universal)) {
    an_ifc_source_location field = get_ifc_while(universal);

    db_print_indent(indent);
    fprintf(f_debug, "while:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_do_while_statement &universal)
/*
Given the universal representation of SyntaxDoWhileStatement, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxDoWhileStatement ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_dynamic_exception_spec &universal,
             unsigned                                   indent)
/*
Given the universal representation of SyntaxDynamicExceptionSpec, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_expander(universal)) {
    an_ifc_source_location field = get_ifc_expander(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expander:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_left_paren(universal)) {
    an_ifc_source_location field = get_ifc_left_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_throw(universal)) {
    an_ifc_source_location field = get_ifc_throw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "throw:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type_list(universal)) {
    an_ifc_syntax_index field = get_ifc_type_list(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type_list:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_dynamic_exception_spec &universal)
/*
Given the universal representation of SyntaxDynamicExceptionSpec, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxDynamicExceptionSpec ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_empty_statement &universal, unsigned indent)
/*
Given the universal representation of SyntaxEmptyStatement, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_empty_statement &universal)
/*
Given the universal representation of SyntaxEmptyStatement, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxEmptyStatement ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_enum_specifier &universal, unsigned indent)
/*
Given the universal representation of SyntaxEnumSpecifier, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_base(universal)) {
    an_ifc_syntax_index field = get_ifc_base(universal);

    db_print_indent(indent);
    fprintf(f_debug, "base:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_class_key(universal)) {
    an_ifc_keyword_syntax field = get_ifc_class_key(universal);

    db_print_indent(indent);
    fprintf(f_debug, "class_key:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_colon(universal)) {
    an_ifc_source_location field = get_ifc_colon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "colon:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_enumerators(universal)) {
    an_ifc_syntax_index field = get_ifc_enumerators(universal);

    db_print_indent(indent);
    fprintf(f_debug, "enumerators:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_left_brace(universal)) {
    an_ifc_source_location field = get_ifc_left_brace(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_brace:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_expr_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_right_brace(universal)) {
    an_ifc_source_location field = get_ifc_right_brace(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_brace:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_enum_specifier &universal)
/*
Given the universal representation of SyntaxEnumSpecifier, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxEnumSpecifier ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_enumerator_definition &universal,
             unsigned                                  indent)
/*
Given the universal representation of SyntaxEnumeratorDefinition, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_equal(universal)) {
    an_ifc_source_location field = get_ifc_equal(universal);

    db_print_indent(indent);
    fprintf(f_debug, "equal:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_enumerator_definition &universal)
/*
Given the universal representation of SyntaxEnumeratorDefinition, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxEnumeratorDefinition ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_exception_declaration &universal,
             unsigned                                  indent)
/*
Given the universal representation of SyntaxExceptionDeclaration, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_declarator(universal)) {
    an_ifc_syntax_index field = get_ifc_declarator(universal);

    db_print_indent(indent);
    fprintf(f_debug, "declarator:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_ellipsis(universal)) {
    an_ifc_source_location field = get_ifc_ellipsis(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ellipsis:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type_specifiers(universal)) {
    an_ifc_syntax_index field = get_ifc_type_specifiers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type_specifiers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_exception_declaration &universal)
/*
Given the universal representation of SyntaxExceptionDeclaration, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxExceptionDeclaration ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_explicit_specifier &universal,
             unsigned                               indent)
/*
Given the universal representation of SyntaxExplicitSpecifier, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_condition(universal)) {
    an_ifc_expr_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_left_paren(universal)) {
    an_ifc_source_location field = get_ifc_left_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_explicit_specifier &universal)
/*
Given the universal representation of SyntaxExplicitSpecifier, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxExplicitSpecifier ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_expression &universal, unsigned indent)
/*
Given the universal representation of SyntaxExpression, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_expression(universal)) {
    an_ifc_expr_index field = get_ifc_expression(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expression:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_expression &universal)
/*
Given the universal representation of SyntaxExpression, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "SyntaxExpression ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_expression_statement &universal,
             unsigned                                 indent)
/*
Given the universal representation of SyntaxExpressionStatement, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_expr(universal)) {
    an_ifc_expr_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_expression_statement &universal)
/*
Given the universal representation of SyntaxExpressionStatement, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxExpressionStatement ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_for_range_declaration &universal,
             unsigned                                  indent)
/*
Given the universal representation of SyntaxForRangeDeclaration, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_declarator(universal)) {
    an_ifc_syntax_index field = get_ifc_declarator(universal);

    db_print_indent(indent);
    fprintf(f_debug, "declarator:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_syntax_index field = get_ifc_specifiers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "specifiers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_for_range_declaration &universal)
/*
Given the universal representation of SyntaxForRangeDeclaration, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxForRangeDeclaration ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_for_statement &universal, unsigned indent)
/*
Given the universal representation of SyntaxForStatement, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_syntax_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_condition(universal)) {
    an_ifc_expr_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_continuation(universal)) {
    an_ifc_expr_index field = get_ifc_continuation(universal);

    db_print_indent(indent);
    fprintf(f_debug, "continuation:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_for(universal)) {
    an_ifc_source_location field = get_ifc_for(universal);

    db_print_indent(indent);
    fprintf(f_debug, "for:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_initialization(universal)) {
    an_ifc_syntax_index field = get_ifc_initialization(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initialization:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_left_paren(universal)) {
    an_ifc_source_location field = get_ifc_left_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_for_statement &universal)
/*
Given the universal representation of SyntaxForStatement, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "SyntaxForStatement ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_function_body &universal, unsigned indent)
/*
Given the universal representation of SyntaxFunctionBody, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_assign(universal)) {
    an_ifc_source_location field = get_ifc_assign(universal);

    db_print_indent(indent);
    fprintf(f_debug, "assign:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_generate(universal)) {
    an_ifc_keyword_syntax field = get_ifc_generate(universal);

    db_print_indent(indent);
    fprintf(f_debug, "generate:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_initializers(universal)) {
    an_ifc_syntax_index field = get_ifc_initializers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_stmts(universal)) {
    an_ifc_syntax_index field = get_ifc_stmts(universal);

    db_print_indent(indent);
    fprintf(f_debug, "stmts:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_try_block(universal)) {
    an_ifc_syntax_index field = get_ifc_try_block(universal);

    db_print_indent(indent);
    fprintf(f_debug, "try_block:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_function_body &universal)
/*
Given the universal representation of SyntaxFunctionBody, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "SyntaxFunctionBody ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_function_declarator &universal,
             unsigned                                indent)
/*
Given the universal representation of SyntaxFunctionDeclarator, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_eh_spec(universal)) {
    an_ifc_syntax_index field = get_ifc_eh_spec(universal);

    db_print_indent(indent);
    fprintf(f_debug, "eh_spec:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_ellipsis(universal)) {
    an_ifc_source_location field = get_ifc_ellipsis(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ellipsis:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_left_paren(universal)) {
    an_ifc_source_location field = get_ifc_left_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_parameters(universal)) {
    an_ifc_syntax_index field = get_ifc_parameters(universal);

    db_print_indent(indent);
    fprintf(f_debug, "parameters:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_ref(universal)) {
    an_ifc_source_location field = get_ifc_ref(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ref:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_traits(universal)) {
    an_ifc_function_type_traits_bitfield field = get_ifc_traits(universal);

    fprintf(f_debug, "traits:\n");
    ++indent;
    if (test_bitmask<ifc_fttb_const>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Const\n");
    }  /* if */
    if (test_bitmask<ifc_fttb_lvalue>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Lvalue\n");
    }  /* if */
    if (test_bitmask<ifc_fttb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_fttb_rvalue>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Rvalue\n");
    }  /* if */
    if (test_bitmask<ifc_fttb_volatile>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Volatile\n");
    }  /* if */
    --indent;
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_function_declarator &universal)
/*
Given the universal representation of SyntaxFunctionDeclarator, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxFunctionDeclarator ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_function_definition &universal,
             unsigned                                indent)
/*
Given the universal representation of SyntaxFunctionDefinition, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_assign(universal)) {
    an_ifc_source_location field = get_ifc_assign(universal);

    db_print_indent(indent);
    fprintf(f_debug, "assign:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_initializers(universal)) {
    an_ifc_syntax_index field = get_ifc_initializers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_stmts(universal)) {
    an_ifc_syntax_index field = get_ifc_stmts(universal);

    db_print_indent(indent);
    fprintf(f_debug, "stmts:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_synthesis(universal)) {
    an_ifc_keyword_syntax field = get_ifc_synthesis(universal);

    db_print_indent(indent);
    fprintf(f_debug, "synthesis:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_try_block(universal)) {
    an_ifc_syntax_index field = get_ifc_try_block(universal);

    db_print_indent(indent);
    fprintf(f_debug, "try_block:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_function_definition &universal)
/*
Given the universal representation of SyntaxFunctionDefinition, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxFunctionDefinition ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_function_try_block &universal,
             unsigned                               indent)
/*
Given the universal representation of SyntaxFunctionTryBlock, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_syntax_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_handlers(universal)) {
    an_ifc_syntax_index field = get_ifc_handlers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "handlers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initializers(universal)) {
    an_ifc_syntax_index field = get_ifc_initializers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_function_try_block &universal)
/*
Given the universal representation of SyntaxFunctionTryBlock, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxFunctionTryBlock ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_goto_statement &universal, unsigned indent)
/*
Given the universal representation of SyntaxGotoStatement, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_label(universal)) {
    an_ifc_source_location field = get_ifc_label(universal);

    db_print_indent(indent);
    fprintf(f_debug, "label:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_target(universal)) {
    an_ifc_text_offset field = get_ifc_target(universal);

    db_print_indent(indent);
    fprintf(f_debug, "target: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_goto_statement &universal)
/*
Given the universal representation of SyntaxGotoStatement, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxGotoStatement ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_handler &universal, unsigned indent)
/*
Given the universal representation of SyntaxHandler, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_syntax_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_catch(universal)) {
    an_ifc_source_location field = get_ifc_catch(universal);

    db_print_indent(indent);
    fprintf(f_debug, "catch:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_exception(universal)) {
    an_ifc_syntax_index field = get_ifc_exception(universal);

    db_print_indent(indent);
    fprintf(f_debug, "exception:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_left_paren(universal)) {
    an_ifc_source_location field = get_ifc_left_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_handler &universal)
/*
Given the universal representation of SyntaxHandler, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "SyntaxHandler ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_handler_seq &universal, unsigned indent)
/*
Given the universal representation of SyntaxHandlerSeq, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_handlers(universal)) {
    an_ifc_syntax_index field = get_ifc_handlers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "handlers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_handler_seq &universal)
/*
Given the universal representation of SyntaxHandlerSeq, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "SyntaxHandlerSeq ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_if_statement &universal, unsigned indent)
/*
Given the universal representation of SyntaxIfStatement, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_alternative(universal)) {
    an_ifc_syntax_index field = get_ifc_alternative(universal);

    db_print_indent(indent);
    fprintf(f_debug, "alternative:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_condition(universal)) {
    an_ifc_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_consequence(universal)) {
    an_ifc_syntax_index field = get_ifc_consequence(universal);

    db_print_indent(indent);
    fprintf(f_debug, "consequence:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_constexpr(universal)) {
    an_ifc_source_location field = get_ifc_constexpr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "constexpr:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_else(universal)) {
    an_ifc_source_location field = get_ifc_else(universal);

    db_print_indent(indent);
    fprintf(f_debug, "else:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_if(universal)) {
    an_ifc_source_location field = get_ifc_if(universal);

    db_print_indent(indent);
    fprintf(f_debug, "if:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_initialization(universal)) {
    an_ifc_syntax_index field = get_ifc_initialization(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initialization:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_if_statement &universal)
/*
Given the universal representation of SyntaxIfStatement, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "SyntaxIfStatement ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_init_capture &universal, unsigned indent)
/*
Given the universal representation of SyntaxInitCapture, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_ampersand(universal)) {
    an_ifc_source_location field = get_ifc_ampersand(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ampersand:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_expander(universal)) {
    an_ifc_source_location field = get_ifc_expander(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expander:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_expr_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_init_capture &universal)
/*
Given the universal representation of SyntaxInitCapture, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "SyntaxInitCapture ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_init_declarator &universal, unsigned indent)
/*
Given the universal representation of SyntaxInitDeclarator, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_constraint(universal)) {
    an_ifc_syntax_index field = get_ifc_constraint(universal);

    db_print_indent(indent);
    fprintf(f_debug, "constraint:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_declarator(universal)) {
    an_ifc_syntax_index field = get_ifc_declarator(universal);

    db_print_indent(indent);
    fprintf(f_debug, "declarator:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_init_declarator &universal)
/*
Given the universal representation of SyntaxInitDeclarator, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxInitDeclarator ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_init_statement &universal, unsigned indent)
/*
Given the universal representation of SyntaxInitStatement, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_init(universal)) {
    an_ifc_syntax_index field = get_ifc_init(universal);

    db_print_indent(indent);
    fprintf(f_debug, "init:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_init_statement &universal)
/*
Given the universal representation of SyntaxInitStatement, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxInitStatement ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_labeled_statement &universal, unsigned indent)
/*
Given the universal representation of SyntaxLabeledStatement, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_label(universal)) {
    an_ifc_expr_index field = get_ifc_label(universal);

    db_print_indent(indent);
    fprintf(f_debug, "label:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_keyword_sort field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_sort(universal)) {
    an_ifc_label_sort field = get_ifc_sort(universal);

    db_print_indent(indent);
    fprintf(f_debug, "sort: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_stmt(universal)) {
    an_ifc_syntax_index field = get_ifc_stmt(universal);

    db_print_indent(indent);
    fprintf(f_debug, "stmt:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_labeled_statement &universal)
/*
Given the universal representation of SyntaxLabeledStatement, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxLabeledStatement ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_lambda_declarator &universal, unsigned indent)
/*
Given the universal representation of SyntaxLambdaDeclarator, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_eh_spec(universal)) {
    an_ifc_syntax_index field = get_ifc_eh_spec(universal);

    db_print_indent(indent);
    fprintf(f_debug, "eh_spec:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_expander(universal)) {
    an_ifc_source_location field = get_ifc_expander(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expander:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_left_paren(universal)) {
    an_ifc_source_location field = get_ifc_left_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_modifier(universal)) {
    an_ifc_keyword_sort field = get_ifc_modifier(universal);

    db_print_indent(indent);
    fprintf(f_debug, "modifier: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_parameters(universal)) {
    an_ifc_syntax_index field = get_ifc_parameters(universal);

    db_print_indent(indent);
    fprintf(f_debug, "parameters:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_trailing_target(universal)) {
    an_ifc_syntax_index field = get_ifc_trailing_target(universal);

    db_print_indent(indent);
    fprintf(f_debug, "trailing_target:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_lambda_declarator &universal)
/*
Given the universal representation of SyntaxLambdaDeclarator, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxLambdaDeclarator ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_lambda_introducer &universal, unsigned indent)
/*
Given the universal representation of SyntaxLambdaIntroducer, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_captures(universal)) {
    an_ifc_syntax_index field = get_ifc_captures(universal);

    db_print_indent(indent);
    fprintf(f_debug, "captures:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_left_bracket(universal)) {
    an_ifc_source_location field = get_ifc_left_bracket(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_bracket:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_right_bracket(universal)) {
    an_ifc_source_location field = get_ifc_right_bracket(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_bracket:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_lambda_introducer &universal)
/*
Given the universal representation of SyntaxLambdaIntroducer, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxLambdaIntroducer ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_mem_initializer &universal, unsigned indent)
/*
Given the universal representation of SyntaxMemInitializer, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_expander(universal)) {
    an_ifc_source_location field = get_ifc_expander(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expander:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_member(universal)) {
    an_ifc_expr_index field = get_ifc_member(universal);

    db_print_indent(indent);
    fprintf(f_debug, "member:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_mem_initializer &universal)
/*
Given the universal representation of SyntaxMemInitializer, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxMemInitializer ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_member_declaration &universal,
             unsigned                               indent)
/*
Given the universal representation of SyntaxMemberDeclaration, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_decl_specifiers(universal)) {
    an_ifc_syntax_index field = get_ifc_decl_specifiers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl_specifiers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_declarations(universal)) {
    an_ifc_syntax_index field = get_ifc_declarations(universal);

    db_print_indent(indent);
    fprintf(f_debug, "declarations:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_member_declaration &universal)
/*
Given the universal representation of SyntaxMemberDeclaration, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxMemberDeclaration ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_member_declarator &universal, unsigned indent)
/*
Given the universal representation of SyntaxMemberDeclarator, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_bitwidth(universal)) {
    an_ifc_expr_index field = get_ifc_bitwidth(universal);

    db_print_indent(indent);
    fprintf(f_debug, "bitwidth:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_colon(universal)) {
    an_ifc_source_location field = get_ifc_colon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "colon:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_constraint(universal)) {
    an_ifc_syntax_index field = get_ifc_constraint(universal);

    db_print_indent(indent);
    fprintf(f_debug, "constraint:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_declarator(universal)) {
    an_ifc_syntax_index field = get_ifc_declarator(universal);

    db_print_indent(indent);
    fprintf(f_debug, "declarator:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_member_declarator &universal)
/*
Given the universal representation of SyntaxMemberDeclarator, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxMemberDeclarator ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_member_function_declaration &universal,
             unsigned                                        indent)
/*
Given the universal representation of SyntaxMemberFunctionDeclaration, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_definition(universal)) {
    an_ifc_syntax_index field = get_ifc_definition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "definition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_member_function_declaration &universal)
/*
Given the universal representation of SyntaxMemberFunctionDeclaration, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "======================= ");
  fprintf(f_debug, "SyntaxMemberFunctionDeclaration ");
  fprintf(f_debug, "========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_member_specification &universal,
             unsigned                                 indent)
/*
Given the universal representation of SyntaxMemberSpecification, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_member_declarations(universal)) {
    an_ifc_syntax_index field = get_ifc_member_declarations(universal);

    db_print_indent(indent);
    fprintf(f_debug, "member_declarations:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_member_specification &universal)
/*
Given the universal representation of SyntaxMemberSpecification, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxMemberSpecification ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_namespace_alias_definition &universal,
             unsigned                                       indent)
/*
Given the universal representation of SyntaxNamespaceAliasDefinition, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_assign(universal)) {
    an_ifc_source_location field = get_ifc_assign(universal);

    db_print_indent(indent);
    fprintf(f_debug, "assign:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_expr_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_namespace_kw(universal)) {
    an_ifc_source_location field = get_ifc_namespace_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "namespace_kw:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_target(universal)) {
    an_ifc_expr_index field = get_ifc_target(universal);

    db_print_indent(indent);
    fprintf(f_debug, "target:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_namespace_alias_definition &universal)
/*
Given the universal representation of SyntaxNamespaceAliasDefinition, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "======================== ");
  fprintf(f_debug, "SyntaxNamespaceAliasDefinition ");
  fprintf(f_debug, "========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_nested_requirement &universal,
             unsigned                               indent)
/*
Given the universal representation of SyntaxNestedRequirement, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_condition(universal)) {
    an_ifc_expr_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_nested_requirement &universal)
/*
Given the universal representation of SyntaxNestedRequirement, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxNestedRequirement ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_new_declarator &universal, unsigned indent)
/*
Given the universal representation of SyntaxNewDeclarator, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_declarator(universal)) {
    an_ifc_syntax_index field = get_ifc_declarator(universal);

    db_print_indent(indent);
    fprintf(f_debug, "declarator:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_new_declarator &universal)
/*
Given the universal representation of SyntaxNewDeclarator, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxNewDeclarator ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_noexcept_specification &universal,
             unsigned                                   indent)
/*
Given the universal representation of SyntaxNoexceptSpecification, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_expr(universal)) {
    an_ifc_syntax_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_left_paren(universal)) {
    an_ifc_source_location field = get_ifc_left_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_noexcept_specification &universal)
/*
Given the universal representation of SyntaxNoexceptSpecification, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================= ");
  fprintf(f_debug, "SyntaxNoexceptSpecification ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_non_type_template_argument &universal,
             unsigned                                       indent)
/*
Given the universal representation of SyntaxNonTypeTemplateArgument, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_argument(universal)) {
    an_ifc_expr_index field = get_ifc_argument(universal);

    db_print_indent(indent);
    fprintf(f_debug, "argument:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_ellipsis(universal)) {
    an_ifc_source_location field = get_ifc_ellipsis(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ellipsis:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_non_type_template_argument &universal)
/*
Given the universal representation of SyntaxNonTypeTemplateArgument, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "======================== ");
  fprintf(f_debug, "SyntaxNonTypeTemplateArgument ");
  fprintf(f_debug, "=========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_parameter_declarator &universal,
             unsigned                                 indent)
/*
Given the universal representation of SyntaxParameterDeclarator, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_decl_specifiers(universal)) {
    an_ifc_syntax_index field = get_ifc_decl_specifiers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl_specifiers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_declarator(universal)) {
    an_ifc_syntax_index field = get_ifc_declarator(universal);

    db_print_indent(indent);
    fprintf(f_debug, "declarator:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_default_expr(universal)) {
    an_ifc_expr_index field = get_ifc_default_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "default_expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_sort(universal)) {
    an_ifc_parameter_sort field = get_ifc_sort(universal);

    db_print_indent(indent);
    fprintf(f_debug, "sort: %s\n", str_for(field));
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_parameter_declarator &universal)
/*
Given the universal representation of SyntaxParameterDeclarator, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxParameterDeclarator ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_placeholder_type_specifier &universal,
             unsigned                                       indent)
/*
Given the universal representation of SyntaxPlaceholderTypeSpecifier, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_basis(universal)) {
    an_ifc_type_basis_sort field = get_ifc_basis(universal);

    db_print_indent(indent);
    fprintf(f_debug, "basis: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_constraint(universal)) {
    an_ifc_expr_index field = get_ifc_constraint(universal);

    db_print_indent(indent);
    fprintf(f_debug, "constraint:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_keyword(universal)) {
    an_ifc_source_location field = get_ifc_keyword(universal);

    db_print_indent(indent);
    fprintf(f_debug, "keyword:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_placeholder_type_specifier &universal)
/*
Given the universal representation of SyntaxPlaceholderTypeSpecifier, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "======================== ");
  fprintf(f_debug, "SyntaxPlaceholderTypeSpecifier ");
  fprintf(f_debug, "========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_pointer_declarator &universal,
             unsigned                               indent)
/*
Given the universal representation of SyntaxPointerDeclarator, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_callable(universal)) {
    an_ifc_bool field = get_ifc_callable(universal);

    db_print_indent(indent);
    fprintf(f_debug, "callable: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_convention(universal)) {
    an_ifc_calling_convention_sort field = get_ifc_convention(universal);

    db_print_indent(indent);
    fprintf(f_debug, "convention: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_next(universal)) {
    an_ifc_syntax_index field = get_ifc_next(universal);

    db_print_indent(indent);
    fprintf(f_debug, "next:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_qualifiers(universal)) {
    an_ifc_qualifier_bitfield field = get_ifc_qualifiers(universal);

    fprintf(f_debug, "qualifiers:\n");
    ++indent;
    if (test_bitmask<ifc_qb_const>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Const\n");
    }  /* if */
    if (test_bitmask<ifc_qb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_qb_restrict>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Restrict\n");
    }  /* if */
    if (test_bitmask<ifc_qb_volatile>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Volatile\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_sort(universal)) {
    an_ifc_pointer_declarator_sort field = get_ifc_sort(universal);

    db_print_indent(indent);
    fprintf(f_debug, "sort: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_whole(universal)) {
    an_ifc_syntax_index field = get_ifc_whole(universal);

    db_print_indent(indent);
    fprintf(f_debug, "whole:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_pointer_declarator &universal)
/*
Given the universal representation of SyntaxPointerDeclarator, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxPointerDeclarator ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_range_based_for_statement &universal,
             unsigned                                      indent)
/*
Given the universal representation of SyntaxRangeBasedForStatement, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_syntax_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_colon(universal)) {
    an_ifc_source_location field = get_ifc_colon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "colon:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_decl(universal)) {
    an_ifc_syntax_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_for(universal)) {
    an_ifc_source_location field = get_ifc_for(universal);

    db_print_indent(indent);
    fprintf(f_debug, "for:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_init(universal)) {
    an_ifc_syntax_index field = get_ifc_init(universal);

    db_print_indent(indent);
    fprintf(f_debug, "init:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_initializer(universal)) {
    an_ifc_syntax_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_left_paren(universal)) {
    an_ifc_source_location field = get_ifc_left_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_range_based_for_statement &universal)
/*
Given the universal representation of SyntaxRangeBasedForStatement, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================= ");
  fprintf(f_debug, "SyntaxRangeBasedForStatement ");
  fprintf(f_debug, "=========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_requirement_body &universal, unsigned indent)
/*
Given the universal representation of SyntaxRequirementBody, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_requirements(universal)) {
    an_ifc_syntax_index field = get_ifc_requirements(universal);

    db_print_indent(indent);
    fprintf(f_debug, "requirements:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_right_curly(universal)) {
    an_ifc_source_location field = get_ifc_right_curly(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_curly:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_requirement_body &universal)
/*
Given the universal representation of SyntaxRequirementBody, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxRequirementBody ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_requires_clause &universal, unsigned indent)
/*
Given the universal representation of SyntaxRequiresClause, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_condition(universal)) {
    an_ifc_expr_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_requires_clause &universal)
/*
Given the universal representation of SyntaxRequiresClause, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxRequiresClause ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_return_statement &universal, unsigned indent)
/*
Given the universal representation of SyntaxReturnStatement, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_expr(universal)) {
    an_ifc_expr_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_return(universal)) {
    an_ifc_source_location field = get_ifc_return(universal);

    db_print_indent(indent);
    fprintf(f_debug, "return:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_sort(universal)) {
    an_ifc_return_sort field = get_ifc_sort(universal);

    db_print_indent(indent);
    fprintf(f_debug, "sort: %s\n", str_for(field));
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_return_statement &universal)
/*
Given the universal representation of SyntaxReturnStatement, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxReturnStatement ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_seh_except &universal, unsigned indent)
/*
Given the universal representation of SyntaxSEHExcept, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_syntax_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_condition(universal)) {
    an_ifc_expr_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_except_kw(universal)) {
    an_ifc_source_location field = get_ifc_except_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "except_kw:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_left_paren(universal)) {
    an_ifc_source_location field = get_ifc_left_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_seh_except &universal)
/*
Given the universal representation of SyntaxSEHExcept, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "SyntaxSEHExcept ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_seh_finally &universal, unsigned indent)
/*
Given the universal representation of SyntaxSEHFinally, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_syntax_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_finally_kw(universal)) {
    an_ifc_source_location field = get_ifc_finally_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "finally_kw:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_seh_finally &universal)
/*
Given the universal representation of SyntaxSEHFinally, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "SyntaxSEHFinally ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_seh_leave &universal, unsigned indent)
/*
Given the universal representation of SyntaxSEHLeave, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_leave_kw(universal)) {
    an_ifc_source_location field = get_ifc_leave_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "leave_kw:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_seh_leave &universal)
/*
Given the universal representation of SyntaxSEHLeave, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "SyntaxSEHLeave ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_seh_try &universal, unsigned indent)
/*
Given the universal representation of SyntaxSEHTry, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_syntax_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_handler(universal)) {
    an_ifc_syntax_index field = get_ifc_handler(universal);

    db_print_indent(indent);
    fprintf(f_debug, "handler:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_try_kw(universal)) {
    an_ifc_source_location field = get_ifc_try_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "try_kw:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_seh_try &universal)
/*
Given the universal representation of SyntaxSEHTry, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "SyntaxSEHTry ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_simple_capture &universal, unsigned indent)
/*
Given the universal representation of SyntaxSimpleCapture, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_ampersand(universal)) {
    an_ifc_source_location field = get_ifc_ampersand(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ampersand:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_expander(universal)) {
    an_ifc_source_location field = get_ifc_expander(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expander:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_expr_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_simple_capture &universal)
/*
Given the universal representation of SyntaxSimpleCapture, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxSimpleCapture ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_simple_declaration &universal,
             unsigned                               indent)
/*
Given the universal representation of SyntaxSimpleDeclaration, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_decl_specifiers(universal)) {
    an_ifc_syntax_index field = get_ifc_decl_specifiers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl_specifiers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_declarators(universal)) {
    an_ifc_syntax_index field = get_ifc_declarators(universal);

    db_print_indent(indent);
    fprintf(f_debug, "declarators:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_simple_declaration &universal)
/*
Given the universal representation of SyntaxSimpleDeclaration, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxSimpleDeclaration ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_simple_requirement &universal,
             unsigned                               indent)
/*
Given the universal representation of SyntaxSimpleRequirement, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_condition(universal)) {
    an_ifc_expr_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_simple_requirement &universal)
/*
Given the universal representation of SyntaxSimpleRequirement, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxSimpleRequirement ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_simple_type_specifier &universal,
             unsigned                                  indent)
/*
Given the universal representation of SyntaxSimpleTypeSpecifier, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_expr(universal)) {
    an_ifc_expr_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_simple_type_specifier &universal)
/*
Given the universal representation of SyntaxSimpleTypeSpecifier, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxSimpleTypeSpecifier ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_statement_seq &universal, unsigned indent)
/*
Given the universal representation of SyntaxStatementSeq, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_stmts(universal)) {
    an_ifc_syntax_index field = get_ifc_stmts(universal);

    db_print_indent(indent);
    fprintf(f_debug, "stmts:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_statement_seq &universal)
/*
Given the universal representation of SyntaxStatementSeq, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "SyntaxStatementSeq ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_static_assert_declaration &universal,
             unsigned                                      indent)
/*
Given the universal representation of SyntaxStaticAssertDeclaration, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_condition(universal)) {
    an_ifc_expr_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_left_paren(universal)) {
    an_ifc_source_location field = get_ifc_left_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_message(universal)) {
    an_ifc_expr_index field = get_ifc_message(universal);

    db_print_indent(indent);
    fprintf(f_debug, "message:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_static_assert_declaration &universal)
/*
Given the universal representation of SyntaxStaticAssertDeclaration, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "======================== ");
  fprintf(f_debug, "SyntaxStaticAssertDeclaration ");
  fprintf(f_debug, "=========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_structured_binding_declaration &universal,
             unsigned                                           indent)
/*
Given the universal representation of SyntaxStructuredBindingDeclaration, print
a diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_initializer(universal)) {
    an_ifc_expr_index field = get_ifc_initializer(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializer:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_names(universal)) {
    an_ifc_syntax_index field = get_ifc_names(universal);

    db_print_indent(indent);
    fprintf(f_debug, "names:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_ref(universal)) {
    an_ifc_source_location field = get_ifc_ref(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ref:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_specifiers(universal)) {
    an_ifc_syntax_index field = get_ifc_specifiers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "specifiers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_structured_binding_declaration &universal)
/*
Given the universal representation of SyntaxStructuredBindingDeclaration, print
a diagnostic textual representation.
*/
{
  fprintf(f_debug, "====================== ");
  fprintf(f_debug, "SyntaxStructuredBindingDeclaration ");
  fprintf(f_debug, "======================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_structured_binding_identifier &universal,
             unsigned                                          indent)
/*
Given the universal representation of SyntaxStructuredBindingIdentifier, print
a diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_expr_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_structured_binding_identifier &universal)
/*
Given the universal representation of SyntaxStructuredBindingIdentifier, print
a diagnostic textual representation.
*/
{
  fprintf(f_debug, "====================== ");
  fprintf(f_debug, "SyntaxStructuredBindingIdentifier ");
  fprintf(f_debug, "=======================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_super &universal, unsigned indent)
/*
Given the universal representation of SyntaxSuper, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_super &universal)
/*
Given the universal representation of SyntaxSuper, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "SyntaxSuper ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_switch_statement &universal, unsigned indent)
/*
Given the universal representation of SyntaxSwitchStatement, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_syntax_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_condition(universal)) {
    an_ifc_syntax_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_init(universal)) {
    an_ifc_syntax_index field = get_ifc_init(universal);

    db_print_indent(indent);
    fprintf(f_debug, "init:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_switch(universal)) {
    an_ifc_source_location field = get_ifc_switch(universal);

    db_print_indent(indent);
    fprintf(f_debug, "switch:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_switch_statement &universal)
/*
Given the universal representation of SyntaxSwitchStatement, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxSwitchStatement ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_template_argument_list &universal,
             unsigned                                   indent)
/*
Given the universal representation of SyntaxTemplateArgumentList, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_arguments(universal)) {
    an_ifc_syntax_index field = get_ifc_arguments(universal);

    db_print_indent(indent);
    fprintf(f_debug, "arguments:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_left_angle(universal)) {
    an_ifc_source_location field = get_ifc_left_angle(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_angle:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_right_angle(universal)) {
    an_ifc_source_location field = get_ifc_right_angle(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_angle:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_template_argument_list &universal)
/*
Given the universal representation of SyntaxTemplateArgumentList, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxTemplateArgumentList ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_template_declaration &universal,
             unsigned                                 indent)
/*
Given the universal representation of SyntaxTemplateDeclaration, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_parameters(universal)) {
    an_ifc_syntax_index field = get_ifc_parameters(universal);

    db_print_indent(indent);
    fprintf(f_debug, "parameters:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_subject(universal)) {
    an_ifc_syntax_index field = get_ifc_subject(universal);

    db_print_indent(indent);
    fprintf(f_debug, "subject:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_template_declaration &universal)
/*
Given the universal representation of SyntaxTemplateDeclaration, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxTemplateDeclaration ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_template_id &universal, unsigned indent)
/*
Given the universal representation of SyntaxTemplateId, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_arguments(universal)) {
    an_ifc_syntax_index field = get_ifc_arguments(universal);

    db_print_indent(indent);
    fprintf(f_debug, "arguments:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_syntax_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_symbol(universal)) {
    an_ifc_expr_index field = get_ifc_symbol(universal);

    db_print_indent(indent);
    fprintf(f_debug, "symbol:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_template_kw(universal)) {
    an_ifc_source_location field = get_ifc_template_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "template_kw:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_template_id &universal)
/*
Given the universal representation of SyntaxTemplateId, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "SyntaxTemplateId ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_template_parameter_list &universal,
             unsigned                                    indent)
/*
Given the universal representation of SyntaxTemplateParameterList, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_clause(universal)) {
    an_ifc_syntax_index field = get_ifc_clause(universal);

    db_print_indent(indent);
    fprintf(f_debug, "clause:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_left_angle(universal)) {
    an_ifc_source_location field = get_ifc_left_angle(universal);

    db_print_indent(indent);
    fprintf(f_debug, "left_angle:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_parameters(universal)) {
    an_ifc_syntax_index field = get_ifc_parameters(universal);

    db_print_indent(indent);
    fprintf(f_debug, "parameters:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_right_angle(universal)) {
    an_ifc_source_location field = get_ifc_right_angle(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_angle:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_template_parameter_list &universal)
/*
Given the universal representation of SyntaxTemplateParameterList, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================= ");
  fprintf(f_debug, "SyntaxTemplateParameterList ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_template_template_parameter &universal,
             unsigned                                        indent)
/*
Given the universal representation of SyntaxTemplateTemplateParameter, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_argument(universal)) {
    an_ifc_syntax_index field = get_ifc_argument(universal);

    db_print_indent(indent);
    fprintf(f_debug, "argument:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_ellipsis(universal)) {
    an_ifc_source_location field = get_ifc_ellipsis(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ellipsis:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_key(universal)) {
    an_ifc_keyword_syntax field = get_ifc_key(universal);

    db_print_indent(indent);
    fprintf(f_debug, "key:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_parameters(universal)) {
    an_ifc_syntax_index field = get_ifc_parameters(universal);

    db_print_indent(indent);
    fprintf(f_debug, "parameters:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_template_template_parameter &universal)
/*
Given the universal representation of SyntaxTemplateTemplateParameter, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "======================= ");
  fprintf(f_debug, "SyntaxTemplateTemplateParameter ");
  fprintf(f_debug, "========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_this_capture &universal, unsigned indent)
/*
Given the universal representation of SyntaxThisCapture, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_asterisk(universal)) {
    an_ifc_source_location field = get_ifc_asterisk(universal);

    db_print_indent(indent);
    fprintf(f_debug, "asterisk:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_this_capture &universal)
/*
Given the universal representation of SyntaxThisCapture, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "SyntaxThisCapture ");
  fprintf(f_debug, "===============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_trailing_return_type &universal,
             unsigned                                 indent)
/*
Given the universal representation of SyntaxTrailingReturnType, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_arrow(universal)) {
    an_ifc_source_location field = get_ifc_arrow(universal);

    db_print_indent(indent);
    fprintf(f_debug, "arrow:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_target(universal)) {
    an_ifc_syntax_index field = get_ifc_target(universal);

    db_print_indent(indent);
    fprintf(f_debug, "target:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_trailing_return_type &universal)
/*
Given the universal representation of SyntaxTrailingReturnType, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxTrailingReturnType ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_try_block &universal, unsigned indent)
/*
Given the universal representation of SyntaxTryBlock, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_syntax_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_handlers(universal)) {
    an_ifc_syntax_index field = get_ifc_handlers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "handlers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_try(universal)) {
    an_ifc_source_location field = get_ifc_try(universal);

    db_print_indent(indent);
    fprintf(f_debug, "try:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_try_block &universal)
/*
Given the universal representation of SyntaxTryBlock, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "SyntaxTryBlock ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_tuple &universal, unsigned indent)
/*
Given the universal representation of SyntaxTuple, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_tuple &universal)
/*
Given the universal representation of SyntaxTuple, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "SyntaxTuple ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_type_id &universal, unsigned indent)
/*
Given the universal representation of SyntaxTypeId, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_abstract_declarator(universal)) {
    an_ifc_syntax_index field = get_ifc_abstract_declarator(universal);

    db_print_indent(indent);
    fprintf(f_debug, "abstract_declarator:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type_specifier(universal)) {
    an_ifc_syntax_index field = get_ifc_type_specifier(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type_specifier:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_type_id &universal)
/*
Given the universal representation of SyntaxTypeId, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "SyntaxTypeId ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_type_id_list_element &universal,
             unsigned                                 indent)
/*
Given the universal representation of SyntaxTypeIdListElement, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_ellipsis(universal)) {
    an_ifc_source_location field = get_ifc_ellipsis(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ellipsis:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type_id(universal)) {
    an_ifc_syntax_index field = get_ifc_type_id(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type_id:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_type_id_list_element &universal)
/*
Given the universal representation of SyntaxTypeIdListElement, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxTypeIdListElement ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_type_requirement &universal, unsigned indent)
/*
Given the universal representation of SyntaxTypeRequirement, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_expr_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_type_requirement &universal)
/*
Given the universal representation of SyntaxTypeRequirement, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxTypeRequirement ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_type_specifier_seq &universal,
             unsigned                               indent)
/*
Given the universal representation of SyntaxTypeSpecifierSeq, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_qualifiers(universal)) {
    an_ifc_qualifier_bitfield field = get_ifc_qualifiers(universal);

    fprintf(f_debug, "qualifiers:\n");
    ++indent;
    if (test_bitmask<ifc_qb_const>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Const\n");
    }  /* if */
    if (test_bitmask<ifc_qb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_qb_restrict>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Restrict\n");
    }  /* if */
    if (test_bitmask<ifc_qb_volatile>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Volatile\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_type_name(universal)) {
    an_ifc_syntax_index field = get_ifc_type_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type_name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_unhashed(universal)) {
    an_ifc_bool field = get_ifc_unhashed(universal);

    db_print_indent(indent);
    fprintf(f_debug, "unhashed: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_type_specifier_seq &universal)
/*
Given the universal representation of SyntaxTypeSpecifierSeq, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxTypeSpecifierSeq ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_type_template_argument &universal,
             unsigned                                   indent)
/*
Given the universal representation of SyntaxTypeTemplateArgument, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_argument(universal)) {
    an_ifc_syntax_index field = get_ifc_argument(universal);

    db_print_indent(indent);
    fprintf(f_debug, "argument:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_ellipsis(universal)) {
    an_ifc_source_location field = get_ifc_ellipsis(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ellipsis:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_type_template_argument &universal)
/*
Given the universal representation of SyntaxTypeTemplateArgument, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxTypeTemplateArgument ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_type_template_parameter &universal,
             unsigned                                    indent)
/*
Given the universal representation of SyntaxTypeTemplateParameter, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_argument(universal)) {
    an_ifc_syntax_index field = get_ifc_argument(universal);

    db_print_indent(indent);
    fprintf(f_debug, "argument:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_constraint(universal)) {
    an_ifc_syntax_index field = get_ifc_constraint(universal);

    db_print_indent(indent);
    fprintf(f_debug, "constraint:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_ellipsis(universal)) {
    an_ifc_source_location field = get_ifc_ellipsis(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ellipsis:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_text_offset field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_type_template_parameter &universal)
/*
Given the universal representation of SyntaxTypeTemplateParameter, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================= ");
  fprintf(f_debug, "SyntaxTypeTemplateParameter ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_type_trait_intrinsic &universal,
             unsigned                                 indent)
/*
Given the universal representation of SyntaxTypeTraitIntrinsic, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_arguments(universal)) {
    an_ifc_syntax_index field = get_ifc_arguments(universal);

    db_print_indent(indent);
    fprintf(f_debug, "arguments:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_intrinsic(universal)) {
    an_ifc_operator_category field = get_ifc_intrinsic(universal);

    db_print_indent(indent);
    fprintf(f_debug, "intrinsic:\n");
    db_print_indent(indent);
    fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
    ++indent;
    switch (field.sort) {
      case ifc_os_dyadic_operator:
        { an_ifc_dyadic_operator_sort &do_ref = field.variant.dyadic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "dyadic_operator: %s\n", str_for(do_ref));
        }
        break;
      case ifc_os_monadic_operator:
        { an_ifc_monadic_operator_sort &mo_ref =
                                                field.variant.monadic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "monadic_operator: %s\n", str_for(mo_ref));
        }
        break;
      case ifc_os_niladic_operator:
        { an_ifc_niladic_operator_sort &no_ref =
                                                field.variant.niladic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "niladic_operator: %s\n", str_for(no_ref));
        }
        break;
      case ifc_os_storage_instruction_operator:
        { an_ifc_storage_instruction_operator_sort &sio_ref =
                                    field.variant.storage_instruction_operator;

          db_print_indent(indent);
          fprintf(f_debug, "storage_instruction_operator: %s\n",
                  str_for(sio_ref));
        }
        break;
      case ifc_os_triadic_operator:
        { an_ifc_triadic_operator_sort &to_ref =
                                                field.variant.triadic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "triadic_operator: %s\n", str_for(to_ref));
        }
        break;
      case ifc_os_variadic_operator:
        { an_ifc_variadic_operator_sort &vo_ref =
                                               field.variant.variadic_operator;

          db_print_indent(indent);
          fprintf(f_debug, "variadic_operator: %s\n", str_for(vo_ref));
        }
        break;
      default_is_unexpected();
    }  /* switch */
    --indent;
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_type_trait_intrinsic &universal)
/*
Given the universal representation of SyntaxTypeTraitIntrinsic, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "SyntaxTypeTraitIntrinsic ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_unary_fold_expression &universal,
             unsigned                                  indent)
/*
Given the universal representation of SyntaxUnaryFoldExpression, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_direction(universal)) {
    an_ifc_fold_direction_sort field = get_ifc_direction(universal);

    db_print_indent(indent);
    fprintf(f_debug, "direction: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_dyad(universal)) {
    an_ifc_dyadic_operator_sort field = get_ifc_dyad(universal);

    db_print_indent(indent);
    fprintf(f_debug, "dyad: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_ellipsis(universal)) {
    an_ifc_source_location field = get_ifc_ellipsis(universal);

    db_print_indent(indent);
    fprintf(f_debug, "ellipsis:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_glyph_locus(universal)) {
    an_ifc_source_location field = get_ifc_glyph_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "glyph_locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_operand(universal)) {
    an_ifc_expr_index field = get_ifc_operand(universal);

    db_print_indent(indent);
    fprintf(f_debug, "operand:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_right_paren(universal)) {
    an_ifc_source_location field = get_ifc_right_paren(universal);

    db_print_indent(indent);
    fprintf(f_debug, "right_paren:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_unary_fold_expression &universal)
/*
Given the universal representation of SyntaxUnaryFoldExpression, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxUnaryFoldExpression ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_using_declaration &universal, unsigned indent)
/*
Given the universal representation of SyntaxUsingDeclaration, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_declarators(universal)) {
    an_ifc_syntax_index field = get_ifc_declarators(universal);

    db_print_indent(indent);
    fprintf(f_debug, "declarators:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_keyword(universal)) {
    an_ifc_source_location field = get_ifc_keyword(universal);

    db_print_indent(indent);
    fprintf(f_debug, "keyword:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_using_declaration &universal)
/*
Given the universal representation of SyntaxUsingDeclaration, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxUsingDeclaration ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_using_declarator &universal, unsigned indent)
/*
Given the universal representation of SyntaxUsingDeclarator, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_comma(universal)) {
    an_ifc_source_location field = get_ifc_comma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "comma:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_expander(universal)) {
    an_ifc_source_location field = get_ifc_expander(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expander:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_qualified_name(universal)) {
    an_ifc_expr_index field = get_ifc_qualified_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "qualified_name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_typename_kw(universal)) {
    an_ifc_source_location field = get_ifc_typename_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "typename_kw:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_using_declarator &universal)
/*
Given the universal representation of SyntaxUsingDeclarator, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================ ");
  fprintf(f_debug, "SyntaxUsingDeclarator ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_using_directive &universal, unsigned indent)
/*
Given the universal representation of SyntaxUsingDirective, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_namespace_kw(universal)) {
    an_ifc_source_location field = get_ifc_namespace_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "namespace_kw:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_qualified_name(universal)) {
    an_ifc_expr_index field = get_ifc_qualified_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "qualified_name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_using_kw(universal)) {
    an_ifc_source_location field = get_ifc_using_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "using_kw:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_using_directive &universal)
/*
Given the universal representation of SyntaxUsingDirective, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxUsingDirective ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_using_enum_declaration &universal,
             unsigned                                   indent)
/*
Given the universal representation of SyntaxUsingEnumDeclaration, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_enum_kw(universal)) {
    an_ifc_source_location field = get_ifc_enum_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "enum_kw:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_name(universal)) {
    an_ifc_expr_index field = get_ifc_name(universal);

    db_print_indent(indent);
    fprintf(f_debug, "name:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_semicolon(universal)) {
    an_ifc_source_location field = get_ifc_semicolon(universal);

    db_print_indent(indent);
    fprintf(f_debug, "semicolon:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_using_kw(universal)) {
    an_ifc_source_location field = get_ifc_using_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "using_kw:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_using_enum_declaration &universal)
/*
Given the universal representation of SyntaxUsingEnumDeclaration, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxUsingEnumDeclaration ");
  fprintf(f_debug, "==========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_virtual_specifier_seq &universal,
             unsigned                                  indent)
/*
Given the universal representation of SyntaxVirtualSpecifierSeq, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_final_kw(universal)) {
    an_ifc_source_location field = get_ifc_final_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "final_kw:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_locus(universal)) {
    an_ifc_source_location field = get_ifc_locus(universal);

    db_print_indent(indent);
    fprintf(f_debug, "locus:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_override_kw(universal)) {
    an_ifc_source_location field = get_ifc_override_kw(universal);

    db_print_indent(indent);
    fprintf(f_debug, "override_kw:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_pure(universal)) {
    an_ifc_bool field = get_ifc_pure(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pure: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_virtual_specifier_seq &universal)
/*
Given the universal representation of SyntaxVirtualSpecifierSeq, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "========================== ");
  fprintf(f_debug, "SyntaxVirtualSpecifierSeq ");
  fprintf(f_debug, "===========================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_syntax_while_statement &universal, unsigned indent)
/*
Given the universal representation of SyntaxWhileStatement, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_syntax_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_condition(universal)) {
    an_ifc_expr_index field = get_ifc_condition(universal);

    db_print_indent(indent);
    fprintf(f_debug, "condition:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_pragma(universal)) {
    an_ifc_sentence_index field = get_ifc_pragma(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pragma: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_while(universal)) {
    an_ifc_source_location field = get_ifc_while(universal);

    db_print_indent(indent);
    fprintf(f_debug, "while:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_syntax_while_statement &universal)
/*
Given the universal representation of SyntaxWhileStatement, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "SyntaxWhileStatement ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_trait_alias_template &universal, unsigned indent)
/*
Given the universal representation of TraitAliasTemplate, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_encoded_decl(universal)) {
    an_ifc_encoded_decl_index field = get_ifc_encoded_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_trait(universal)) {
    an_ifc_syntax_index field = get_ifc_trait(universal);

    db_print_indent(indent);
    fprintf(f_debug, "trait:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_trait_alias_template &universal)
/*
Given the universal representation of TraitAliasTemplate, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "TraitAliasTemplate ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_trait_attribute &universal, unsigned indent)
/*
Given the universal representation of TraitAttribute, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_encoded_decl(universal)) {
    an_ifc_encoded_decl_index field = get_ifc_encoded_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_trait(universal)) {
    an_ifc_attr_index field = get_ifc_trait(universal);

    db_print_indent(indent);
    fprintf(f_debug, "trait:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_trait_attribute &universal)
/*
Given the universal representation of TraitAttribute, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "TraitAttribute ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_trait_deduction_guide &universal, unsigned indent)
/*
Given the universal representation of TraitDeductionGuide, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_encoded_decl(universal)) {
    an_ifc_encoded_decl_index field = get_ifc_encoded_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_trait(universal)) {
    an_ifc_decl_index field = get_ifc_trait(universal);

    db_print_indent(indent);
    fprintf(f_debug, "trait:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_trait_deduction_guide &universal)
/*
Given the universal representation of TraitDeductionGuide, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "TraitDeductionGuide ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_trait_deprecated &universal, unsigned indent)
/*
Given the universal representation of TraitDeprecated, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_encoded_decl(universal)) {
    an_ifc_encoded_decl_index field = get_ifc_encoded_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_trait(universal)) {
    an_ifc_text_offset field = get_ifc_trait(universal);

    db_print_indent(indent);
    fprintf(f_debug, "trait: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_trait_deprecated &universal)
/*
Given the universal representation of TraitDeprecated, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "TraitDeprecated ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_trait_friend &universal, unsigned indent)
/*
Given the universal representation of TraitFriend, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_encoded_decl(universal)) {
    an_ifc_encoded_decl_index field = get_ifc_encoded_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_trait(universal)) {
    an_ifc_sequence field = get_ifc_trait(universal);

    db_print_indent(indent);
    fprintf(f_debug, "trait:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_trait_friend &universal)
/*
Given the universal representation of TraitFriend, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "TraitFriend ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_trait_function_definition &universal,
             unsigned                               indent)
/*
Given the universal representation of TraitFunctionDefinition, print a
diagnostic textual representation with the given indent.
*/
{
  if (has_ifc_body(universal)) {
    an_ifc_stmt_index field = get_ifc_body(universal);

    db_print_indent(indent);
    fprintf(f_debug, "body:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_encoded_decl(universal)) {
    an_ifc_encoded_decl_index field = get_ifc_encoded_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_initializers(universal)) {
    an_ifc_expr_index field = get_ifc_initializers(universal);

    db_print_indent(indent);
    fprintf(f_debug, "initializers:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_parameters(universal)) {
    an_ifc_chart_index field = get_ifc_parameters(universal);

    db_print_indent(indent);
    fprintf(f_debug, "parameters:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_trait_function_definition &universal)
/*
Given the universal representation of TraitFunctionDefinition, print a
diagnostic textual representation.
*/
{
  fprintf(f_debug, "=========================== ");
  fprintf(f_debug, "TraitFunctionDefinition ");
  fprintf(f_debug, "============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_trait_msvc_decl_attrs &universal, unsigned indent)
/*
Given the universal representation of TraitMsvcDeclAttrs, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_encoded_decl(universal)) {
    an_ifc_encoded_decl_index field = get_ifc_encoded_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_trait(universal)) {
    an_ifc_attr_index field = get_ifc_trait(universal);

    db_print_indent(indent);
    fprintf(f_debug, "trait:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_trait_msvc_decl_attrs &universal)
/*
Given the universal representation of TraitMsvcDeclAttrs, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================== ");
  fprintf(f_debug, "TraitMsvcDeclAttrs ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_trait_msvc_func_params &universal, unsigned indent)
/*
Given the universal representation of TraitMsvcFuncParams, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_encoded_decl(universal)) {
    an_ifc_encoded_decl_index field = get_ifc_encoded_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_params(universal)) {
    an_ifc_chart_index field = get_ifc_params(universal);

    db_print_indent(indent);
    fprintf(f_debug, "params:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_trait_msvc_func_params &universal)
/*
Given the universal representation of TraitMsvcFuncParams, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "TraitMsvcFuncParams ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_trait_msvc_uuid &universal, unsigned indent)
/*
Given the universal representation of TraitMsvcUuid, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_encoded_decl(universal)) {
    an_ifc_encoded_decl_index field = get_ifc_encoded_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_uuid(universal)) {
    db_print_indent(indent);
    fprintf(f_debug, "uuid: UNIMPLEMENTED\n");
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_trait_msvc_uuid &universal)
/*
Given the universal representation of TraitMsvcUuid, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "TraitMsvcUuid ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_trait_msvc_vendor_trait &universal, unsigned indent)
/*
Given the universal representation of TraitMsvcVendorTrait, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_encoded_decl(universal)) {
    an_ifc_encoded_decl_index field = get_ifc_encoded_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_trait(universal)) {
    an_ifc_msvc_traits_bitfield field = get_ifc_trait(universal);

    fprintf(f_debug, "trait:\n");
    ++indent;
    if (test_bitmask<ifc_mtb_allocate>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Allocate\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_code_segment>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- CodeSegment\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_comdat>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Comdat\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_dll_export>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DllExport\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_dll_import>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- DllImport\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_empty_bases>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- EmptyBases\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_force_inline>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- ForceInline\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_intrinsic_type>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- IntrinsicType\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_naked>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Naked\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_no_alias>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NoAlias\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_no_inline>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- NoInline\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_novtable>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Novtable\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_process>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Process\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_restrict>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Restrict\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_safe_buffers>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- SafeBuffers\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_select_any>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- SelectAny\n");
    }  /* if */
    if (test_bitmask<ifc_mtb_uuid>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Uuid\n");
    }  /* if */
    --indent;
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_trait_msvc_vendor_trait &universal)
/*
Given the universal representation of TraitMsvcVendorTrait, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "TraitMsvcVendorTrait ");
  fprintf(f_debug, "=============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_trait_requires &universal, unsigned indent)
/*
Given the universal representation of TraitRequires, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_encoded_decl(universal)) {
    an_ifc_encoded_decl_index field = get_ifc_encoded_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_trait(universal)) {
    an_ifc_syntax_index field = get_ifc_trait(universal);

    db_print_indent(indent);
    fprintf(f_debug, "trait:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_trait_requires &universal)
/*
Given the universal representation of TraitRequires, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "TraitRequires ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_trait_specialization &universal, unsigned indent)
/*
Given the universal representation of TraitSpecialization, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_encoded_decl(universal)) {
    an_ifc_encoded_decl_index field = get_ifc_encoded_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "encoded_decl: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_trait(universal)) {
    an_ifc_sequence field = get_ifc_trait(universal);

    db_print_indent(indent);
    fprintf(f_debug, "trait:\n");
    db_node(field, indent + 1);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_trait_specialization &universal)
/*
Given the universal representation of TraitSpecialization, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "TraitSpecialization ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_array &universal, unsigned indent)
/*
Given the universal representation of TypeArray, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_element(universal)) {
    an_ifc_type_index field = get_ifc_element(universal);

    db_print_indent(indent);
    fprintf(f_debug, "element:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_extent(universal)) {
    an_ifc_expr_index field = get_ifc_extent(universal);

    db_print_indent(indent);
    fprintf(f_debug, "extent:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_array &universal)
/*
Given the universal representation of TypeArray, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "TypeArray ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_base &universal, unsigned indent)
/*
Given the universal representation of TypeBase, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_access(universal)) {
    an_ifc_access_sort field = get_ifc_access(universal);

    db_print_indent(indent);
    fprintf(f_debug, "access: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_pack_expanded(universal)) {
    an_ifc_bool field = get_ifc_pack_expanded(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pack_expanded: %llu\n",
            (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_shared(universal)) {
    an_ifc_bool field = get_ifc_shared(universal);

    db_print_indent(indent);
    fprintf(f_debug, "shared: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_base &universal)
/*
Given the universal representation of TypeBase, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "TypeBase ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_decltype &universal, unsigned indent)
/*
Given the universal representation of TypeDecltype, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_expr(universal)) {
    an_ifc_syntax_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_decltype &universal)
/*
Given the universal representation of TypeDecltype, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "TypeDecltype ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_designated &universal, unsigned indent)
/*
Given the universal representation of TypeDesignated, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_decl(universal)) {
    an_ifc_decl_index field = get_ifc_decl(universal);

    db_print_indent(indent);
    fprintf(f_debug, "decl:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_designated &universal)
/*
Given the universal representation of TypeDesignated, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "TypeDesignated ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_expansion &universal, unsigned indent)
/*
Given the universal representation of TypeExpansion, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_mode(universal)) {
    an_ifc_expansion_mode_sort field = get_ifc_mode(universal);

    db_print_indent(indent);
    fprintf(f_debug, "mode: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_pack(universal)) {
    an_ifc_type_index field = get_ifc_pack(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pack:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_expansion &universal)
/*
Given the universal representation of TypeExpansion, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "TypeExpansion ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_forall &universal, unsigned indent)
/*
Given the universal representation of TypeForall, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_chart(universal)) {
    an_ifc_chart_index field = get_ifc_chart(universal);

    db_print_indent(indent);
    fprintf(f_debug, "chart:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_subject(universal)) {
    an_ifc_type_index field = get_ifc_subject(universal);

    db_print_indent(indent);
    fprintf(f_debug, "subject:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_forall &universal)
/*
Given the universal representation of TypeForall, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "TypeForall ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_function &universal, unsigned indent)
/*
Given the universal representation of TypeFunction, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_convention(universal)) {
    an_ifc_calling_convention_sort field = get_ifc_convention(universal);

    db_print_indent(indent);
    fprintf(f_debug, "convention: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_eh_spec(universal)) {
    an_ifc_noexcept_specification field = get_ifc_eh_spec(universal);

    db_print_indent(indent);
    fprintf(f_debug, "eh_spec:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_source(universal)) {
    an_ifc_type_index field = get_ifc_source(universal);

    db_print_indent(indent);
    fprintf(f_debug, "source:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_target(universal)) {
    an_ifc_type_index field = get_ifc_target(universal);

    db_print_indent(indent);
    fprintf(f_debug, "target:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_traits(universal)) {
    an_ifc_function_type_traits_bitfield field = get_ifc_traits(universal);

    fprintf(f_debug, "traits:\n");
    ++indent;
    if (test_bitmask<ifc_fttb_const>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Const\n");
    }  /* if */
    if (test_bitmask<ifc_fttb_lvalue>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Lvalue\n");
    }  /* if */
    if (test_bitmask<ifc_fttb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_fttb_rvalue>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Rvalue\n");
    }  /* if */
    if (test_bitmask<ifc_fttb_volatile>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Volatile\n");
    }  /* if */
    --indent;
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_function &universal)
/*
Given the universal representation of TypeFunction, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "TypeFunction ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_fundamental &universal, unsigned indent)
/*
Given the universal representation of TypeFundamental, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_basis(universal)) {
    an_ifc_type_basis_sort field = get_ifc_basis(universal);

    db_print_indent(indent);
    fprintf(f_debug, "basis: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_precision(universal)) {
    an_ifc_type_precision_sort field = get_ifc_precision(universal);

    db_print_indent(indent);
    fprintf(f_debug, "precision: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_sign(universal)) {
    an_ifc_type_sign_sort field = get_ifc_sign(universal);

    db_print_indent(indent);
    fprintf(f_debug, "sign: %s\n", str_for(field));
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_fundamental &universal)
/*
Given the universal representation of TypeFundamental, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "TypeFundamental ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_lvalue_reference &universal, unsigned indent)
/*
Given the universal representation of TypeLvalueReference, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_referee(universal)) {
    an_ifc_type_index field = get_ifc_referee(universal);

    db_print_indent(indent);
    fprintf(f_debug, "referee:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_lvalue_reference &universal)
/*
Given the universal representation of TypeLvalueReference, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "TypeLvalueReference ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_method &universal, unsigned indent)
/*
Given the universal representation of TypeMethod, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_convention(universal)) {
    an_ifc_calling_convention_sort field = get_ifc_convention(universal);

    db_print_indent(indent);
    fprintf(f_debug, "convention: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_eh_spec(universal)) {
    an_ifc_noexcept_specification field = get_ifc_eh_spec(universal);

    db_print_indent(indent);
    fprintf(f_debug, "eh_spec:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_scope(universal)) {
    an_ifc_type_index field = get_ifc_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_source(universal)) {
    an_ifc_type_index field = get_ifc_source(universal);

    db_print_indent(indent);
    fprintf(f_debug, "source:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_target(universal)) {
    an_ifc_type_index field = get_ifc_target(universal);

    db_print_indent(indent);
    fprintf(f_debug, "target:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_traits(universal)) {
    an_ifc_function_type_traits_bitfield field = get_ifc_traits(universal);

    fprintf(f_debug, "traits:\n");
    ++indent;
    if (test_bitmask<ifc_fttb_const>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Const\n");
    }  /* if */
    if (test_bitmask<ifc_fttb_lvalue>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Lvalue\n");
    }  /* if */
    if (test_bitmask<ifc_fttb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_fttb_rvalue>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Rvalue\n");
    }  /* if */
    if (test_bitmask<ifc_fttb_volatile>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Volatile\n");
    }  /* if */
    --indent;
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_method &universal)
/*
Given the universal representation of TypeMethod, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "TypeMethod ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_placeholder &universal, unsigned indent)
/*
Given the universal representation of TypePlaceholder, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_basis(universal)) {
    an_ifc_type_basis_sort field = get_ifc_basis(universal);

    db_print_indent(indent);
    fprintf(f_debug, "basis: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_constraint(universal)) {
    an_ifc_expr_index field = get_ifc_constraint(universal);

    db_print_indent(indent);
    fprintf(f_debug, "constraint:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_elaboration(universal)) {
    an_ifc_type_index field = get_ifc_elaboration(universal);

    db_print_indent(indent);
    fprintf(f_debug, "elaboration:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_placeholder &universal)
/*
Given the universal representation of TypePlaceholder, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "=============================== ");
  fprintf(f_debug, "TypePlaceholder ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_pointer &universal, unsigned indent)
/*
Given the universal representation of TypePointer, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_pointee(universal)) {
    an_ifc_type_index field = get_ifc_pointee(universal);

    db_print_indent(indent);
    fprintf(f_debug, "pointee:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_pointer &universal)
/*
Given the universal representation of TypePointer, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "TypePointer ");
  fprintf(f_debug, "==================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_pointer_to_member &universal, unsigned indent)
/*
Given the universal representation of TypePointerToMember, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_member(universal)) {
    an_ifc_type_index field = get_ifc_member(universal);

    db_print_indent(indent);
    fprintf(f_debug, "member:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
  if (has_ifc_scope(universal)) {
    an_ifc_type_index field = get_ifc_scope(universal);

    db_print_indent(indent);
    fprintf(f_debug, "scope:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_pointer_to_member &universal)
/*
Given the universal representation of TypePointerToMember, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "TypePointerToMember ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_qualified &universal, unsigned indent)
/*
Given the universal representation of TypeQualified, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_qualifiers(universal)) {
    an_ifc_qualifier_bitfield field = get_ifc_qualifiers(universal);

    fprintf(f_debug, "qualifiers:\n");
    ++indent;
    if (test_bitmask<ifc_qb_const>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Const\n");
    }  /* if */
    if (test_bitmask<ifc_qb_none>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- None\n");
    }  /* if */
    if (test_bitmask<ifc_qb_restrict>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Restrict\n");
    }  /* if */
    if (test_bitmask<ifc_qb_volatile>(field)) {
      db_print_indent(indent);
      fprintf(f_debug, "- Volatile\n");
    }  /* if */
    --indent;
  }  /* if */
  if (has_ifc_unqualified(universal)) {
    an_ifc_type_index field = get_ifc_unqualified(universal);

    db_print_indent(indent);
    fprintf(f_debug, "unqualified:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_qualified &universal)
/*
Given the universal representation of TypeQualified, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "TypeQualified ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_rvalue_reference &universal, unsigned indent)
/*
Given the universal representation of TypeRvalueReference, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_referee(universal)) {
    an_ifc_type_index field = get_ifc_referee(universal);

    db_print_indent(indent);
    fprintf(f_debug, "referee:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_rvalue_reference &universal)
/*
Given the universal representation of TypeRvalueReference, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "============================= ");
  fprintf(f_debug, "TypeRvalueReference ");
  fprintf(f_debug, "==============================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_syntactic &universal, unsigned indent)
/*
Given the universal representation of TypeSyntactic, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_expr(universal)) {
    an_ifc_expr_index field = get_ifc_expr(universal);

    db_print_indent(indent);
    fprintf(f_debug, "expr:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_syntactic &universal)
/*
Given the universal representation of TypeSyntactic, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "TypeSyntactic ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_syntax_tree &universal, unsigned indent)
/*
Given the universal representation of TypeSyntaxTree, print a diagnostic
textual representation with the given indent.
*/
{
  if (has_ifc_syntax(universal)) {
    an_ifc_syntax_index field = get_ifc_syntax(universal);

    db_print_indent(indent);
    fprintf(f_debug, "syntax:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_syntax_tree &universal)
/*
Given the universal representation of TypeSyntaxTree, print a diagnostic
textual representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "TypeSyntaxTree ");
  fprintf(f_debug, "================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_tor &universal, unsigned indent)
/*
Given the universal representation of TypeTor, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_convention(universal)) {
    an_ifc_calling_convention_sort field = get_ifc_convention(universal);

    db_print_indent(indent);
    fprintf(f_debug, "convention: %s\n", str_for(field));
  }  /* if */
  if (has_ifc_eh_spec(universal)) {
    an_ifc_noexcept_specification field = get_ifc_eh_spec(universal);

    db_print_indent(indent);
    fprintf(f_debug, "eh_spec:\n");
    db_node(field, indent + 1);
  }  /* if */
  if (has_ifc_source(universal)) {
    an_ifc_type_index field = get_ifc_source(universal);

    db_print_indent(indent);
    fprintf(f_debug, "source:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_tor &universal)
/*
Given the universal representation of TypeTor, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "=================================== ");
  fprintf(f_debug, "TypeTor ");
  fprintf(f_debug, "====================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_tuple &universal, unsigned indent)
/*
Given the universal representation of TypeTuple, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_cardinality(universal)) {
    an_ifc_cardinality field = get_ifc_cardinality(universal);

    db_print_indent(indent);
    fprintf(f_debug, "cardinality: %llu\n", (unsigned long long)field.value);
  }  /* if */
  if (has_ifc_start(universal)) {
    an_ifc_index field = get_ifc_start(universal);

    db_print_indent(indent);
    fprintf(f_debug, "start: %llu\n", (unsigned long long)field.value);
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_tuple &universal)
/*
Given the universal representation of TypeTuple, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================== ");
  fprintf(f_debug, "TypeTuple ");
  fprintf(f_debug, "===================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_typename &universal, unsigned indent)
/*
Given the universal representation of TypeTypename, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_path(universal)) {
    an_ifc_expr_index field = get_ifc_path(universal);

    db_print_indent(indent);
    fprintf(f_debug, "path:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_typename &universal)
/*
Given the universal representation of TypeTypename, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================= ");
  fprintf(f_debug, "TypeTypename ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


void db_node(const an_ifc_type_unaligned &universal, unsigned indent)
/*
Given the universal representation of TypeUnaligned, print a diagnostic textual
representation with the given indent.
*/
{
  if (has_ifc_type(universal)) {
    an_ifc_type_index field = get_ifc_type(universal);

    db_print_indent(indent);
    fprintf(f_debug, "type:");
    if (is_null_index(field)) {
      fprintf(f_debug, " NULL\n");
    } else {
      fprintf(f_debug, "\n");
      db_print_indent(indent);
      fprintf(f_debug, "  sort: %s\n", str_for(field.sort));
      db_print_indent(indent);
      fprintf(f_debug, "  value: %llu\n", (unsigned long long)field.value);
    }  /* if */
  }  /* if */
}  /* db_node */


void db_node(const an_ifc_type_unaligned &universal)
/*
Given the universal representation of TypeUnaligned, print a diagnostic textual
representation.
*/
{
  fprintf(f_debug, "================================ ");
  fprintf(f_debug, "TypeUnaligned ");
  fprintf(f_debug, "=================================\n");
  db_node(universal, 0);
}  /* db_node */


/*
Visitor functions for printing diagnostic textual representations on a given
index.
*/


void db_node_at_idx(an_ifc_attr_index idx)
/*
Given the AttrIndex, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_as_attr_basic:
        { an_ifc_attr_basic universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_as_attr_called:
        { an_ifc_attr_called universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_as_attr_elaborated:
        { an_ifc_attr_elaborated universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_as_attr_expanded:
        { an_ifc_attr_expanded universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_as_attr_factored:
        { an_ifc_attr_factored universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_as_attr_labeled:
        { an_ifc_attr_labeled universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_as_attr_scoped:
        { an_ifc_attr_scoped universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_as_attr_tuple:
        { an_ifc_attr_tuple universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_chart_index idx)
/*
Given the ChartIndex, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_cs_chart_multilevel:
        { an_ifc_chart_multilevel universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_cs_chart_unilevel:
        { an_ifc_chart_unilevel universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_decl_index idx)
/*
Given the DeclIndex, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_ds_decl_alias:
        { an_ifc_decl_alias universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_barren:
        { an_ifc_decl_barren universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_bitfield:
        { an_ifc_decl_bitfield universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_concept:
        { an_ifc_decl_concept universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_constructor:
        { an_ifc_decl_constructor universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_deduction_guide:
        { an_ifc_decl_deduction_guide universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_default_argument:
        { an_ifc_decl_default_argument universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_destructor:
        { an_ifc_decl_destructor universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_enumeration:
        { an_ifc_decl_enumeration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_enumerator:
        { an_ifc_decl_enumerator universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_expansion:
        { an_ifc_decl_expansion universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_explicit_instantiation:
        { an_ifc_decl_explicit_instantiation universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_explicit_specialization:
        { an_ifc_decl_explicit_specialization universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_field:
        { an_ifc_decl_field universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_friend:
        { an_ifc_decl_friend universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_function:
        { an_ifc_decl_function universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_inherited_constructor:
        { an_ifc_decl_inherited_constructor universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_intrinsic:
        { an_ifc_decl_intrinsic universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_method:
        { an_ifc_decl_method universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_output_segment:
        { an_ifc_decl_output_segment universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_parameter:
        { an_ifc_decl_parameter universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_partial_specialization:
        { an_ifc_decl_partial_specialization universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_prolongation:
        { an_ifc_decl_prolongation universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_property:
        { an_ifc_decl_property universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_reference:
        { an_ifc_decl_reference universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_scope:
        { an_ifc_decl_scope universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_specialization:
        { an_ifc_decl_specialization universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_syntax_tree:
        { an_ifc_decl_syntax_tree universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_template:
        { an_ifc_decl_template universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_temploid:
        { an_ifc_decl_temploid universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_tuple:
        { an_ifc_decl_tuple universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_using_declaration:
        { an_ifc_decl_using_declaration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_decl_variable:
        { an_ifc_decl_variable universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_dir_index idx)
/*
Given the DirIndex, print a diagnostic textual representation of the associated
node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_ds_dir_attribute:
        { an_ifc_dir_attribute universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_dir_decl_use:
        { an_ifc_dir_decl_use universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_dir_empty:
        { an_ifc_dir_empty universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_dir_expr:
        { an_ifc_dir_expr universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_dir_pragma:
        { an_ifc_dir_pragma universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_dir_tuple:
        { an_ifc_dir_tuple universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ds_dir_using:
        { an_ifc_dir_using universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_edg_complex_token_index idx)
/*
Given the EdgComplexTokenIndex, print a diagnostic textual representation of
the associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_ects_edg_token_constant:
        { an_ifc_edg_token_constant universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ects_edg_token_identifier:
        { an_ifc_edg_token_identifier universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ects_edg_token_textual:
        { an_ifc_edg_token_textual universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_edg_constant_index idx)
/*
Given the EdgConstantIndex, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_ecs_edg_constant_integer:
        { an_ifc_edg_constant_integer universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_edg_constant_integer_word_offset idx)
/*
Given the EdgConstantIntegerWordOffset, print a diagnostic textual
representation of the associated node.
*/
{
  if (validate(idx)) {
    an_ifc_edg_constant_integer_word universal;

    construct_node_prechecked(&universal, idx);
    db_node(universal);
  } else {
    fputs("Invalid EdgConstantIntegerWord node.\n", f_debug);
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_edg_expr_index idx)
/*
Given the EdgExprIndex, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_ees_edg_expr_template_argument:
        { an_ifc_edg_expr_template_argument universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ees_edg_token_cache:
        { an_ifc_edg_token_cache universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_edg_extension_expr_offset idx)
/*
Given the EdgExtensionExprOffset, print a diagnostic textual representation of
the associated node.
*/
{
  if (validate(idx)) {
    an_ifc_edg_extension_expr universal;

    construct_node_prechecked(&universal, idx);
    db_node(universal);
  } else {
    fputs("Invalid EdgExtensionExpr node.\n", f_debug);
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_edg_extension_type_offset idx)
/*
Given the EdgExtensionTypeOffset, print a diagnostic textual representation of
the associated node.
*/
{
  if (validate(idx)) {
    an_ifc_edg_extension_type universal;

    construct_node_prechecked(&universal, idx);
    db_node(universal);
  } else {
    fputs("Invalid EdgExtensionType node.\n", f_debug);
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_edg_heap_complex_token_offset idx)
/*
Given the EdgHeapComplexTokenOffset, print a diagnostic textual representation
of the associated node.
*/
{
  if (validate(idx)) {
    an_ifc_edg_heap_complex_token universal;

    construct_node_prechecked(&universal, idx);
    db_node(universal);
  } else {
    fputs("Invalid EdgHeapComplexToken node.\n", f_debug);
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_edg_heap_template_argument_offset idx)
/*
Given the EdgHeapTemplateArgumentOffset, print a diagnostic textual
representation of the associated node.
*/
{
  if (validate(idx)) {
    an_ifc_edg_heap_template_argument universal;

    construct_node_prechecked(&universal, idx);
    db_node(universal);
  } else {
    fputs("Invalid EdgHeapTemplateArgument node.\n", f_debug);
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_edg_template_argument_index idx)
/*
Given the EdgTemplateArgumentIndex, print a diagnostic textual representation
of the associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_etas_edg_template_argument_non_type:
        { an_ifc_edg_template_argument_non_type universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_etas_edg_template_argument_template:
        { an_ifc_edg_template_argument_template universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_etas_edg_template_argument_type:
        { an_ifc_edg_template_argument_type universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_edg_token_basic_offset idx)
/*
Given the EdgTokenBasicOffset, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    an_ifc_edg_token_basic universal;

    construct_node_prechecked(&universal, idx);
    db_node(universal);
  } else {
    fputs("Invalid EdgTokenBasic node.\n", f_debug);
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_edg_token_cache_offset idx)
/*
Given the EdgTokenCacheOffset, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    an_ifc_edg_token_cache universal;

    construct_node_prechecked(&universal, idx);
    db_node(universal);
  } else {
    fputs("Invalid EdgTokenCache node.\n", f_debug);
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_edg_type_index idx)
/*
Given the EdgTypeIndex, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_ets_edg_type_float:
        { an_ifc_edg_type_float universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ets_edg_type_substituted:
        { an_ifc_edg_type_substituted universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_expr_index idx)
/*
Given the ExprIndex, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_es_expr_alignof:
        { an_ifc_expr_alignof universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_array_value:
        { an_ifc_expr_array_value universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_assign_initializer:
        { an_ifc_expr_assign_initializer universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_binary_fold:
        { an_ifc_expr_binary_fold universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_call:
        { an_ifc_expr_call universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_cast:
        { an_ifc_expr_cast universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_compound_string:
        { an_ifc_expr_compound_string universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_condition:
        { an_ifc_expr_condition universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_designated_initializer:
        { an_ifc_expr_designated_initializer universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_destructor_call:
        { an_ifc_expr_destructor_call universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_dyad:
        { an_ifc_expr_dyad universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_dynamic_dispatch:
        { an_ifc_expr_dynamic_dispatch universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_empty:
        { an_ifc_expr_empty universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_expansion:
        { an_ifc_expr_expansion universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_expression_list:
        { an_ifc_expr_expression_list universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_function_string:
        { an_ifc_expr_function_string universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_hierarchy_conversion:
        { an_ifc_expr_hierarchy_conversion universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_inheritance_path:
        { an_ifc_expr_inheritance_path universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_initializer:
        { an_ifc_expr_initializer universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_initializer_list:
        { an_ifc_expr_initializer_list universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_label:
        { an_ifc_expr_label universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_lambda:
        { an_ifc_expr_lambda universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_literal:
        { an_ifc_expr_literal universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_member_access:
        { an_ifc_expr_member_access universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_member_initializer:
        { an_ifc_expr_member_initializer universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_monad:
        { an_ifc_expr_monad universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_named_decl:
        { an_ifc_expr_named_decl universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_nullptr:
        { an_ifc_expr_nullptr universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_packed_template_arguments:
        { an_ifc_expr_packed_template_arguments universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_path:
        { an_ifc_expr_path universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_placeholder:
        { an_ifc_expr_placeholder universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_pointer:
        { an_ifc_expr_pointer universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_product_type_value:
        { an_ifc_expr_product_type_value universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_push_state:
        { an_ifc_expr_push_state universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_qualified_name:
        { an_ifc_expr_qualified_name universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_read:
        { an_ifc_expr_read universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_requires:
        { an_ifc_expr_requires universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_simple_identifier:
        { an_ifc_expr_simple_identifier universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_sizeof_type:
        { an_ifc_expr_sizeof_type universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_string:
        { an_ifc_expr_string universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_string_sequence:
        { an_ifc_expr_string_sequence universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_subobject_value:
        { an_ifc_expr_subobject_value universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_sum_type_value:
        { an_ifc_expr_sum_type_value universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_syntax_tree:
        { an_ifc_expr_syntax_tree universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_template_id:
        { an_ifc_expr_template_id universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_template_reference:
        { an_ifc_expr_template_reference universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_temporary:
        { an_ifc_expr_temporary universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_this:
        { an_ifc_expr_this universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_tokens:
        { an_ifc_expr_tokens universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_triad:
        { an_ifc_expr_triad universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_tuple:
        { an_ifc_expr_tuple universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_type:
        { an_ifc_expr_type universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_type_trait_intrinsic:
        { an_ifc_expr_type_trait_intrinsic universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_typeid:
        { an_ifc_expr_typeid universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_unary_fold:
        { an_ifc_expr_unary_fold universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_unqualified_id:
        { an_ifc_expr_unqualified_id universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_unresolved_id:
        { an_ifc_expr_unresolved_id universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_es_expr_virtual_function_conversion:
        { an_ifc_expr_virtual_function_conversion universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_expr_named_decl_offset idx)
/*
Given the ExprNamedDeclOffset, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    an_ifc_expr_named_decl universal;

    construct_node_prechecked(&universal, idx);
    db_node(universal);
  } else {
    fputs("Invalid ExprNamedDecl node.\n", f_debug);
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_form_index idx)
/*
Given the FormIndex, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_fs_form_catenate:
        { an_ifc_form_catenate universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_fs_form_character:
        { an_ifc_form_character universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_fs_form_header:
        { an_ifc_form_header universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_fs_form_identifier:
        { an_ifc_form_identifier universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_fs_form_junk:
        { an_ifc_form_junk universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_fs_form_keyword:
        { an_ifc_form_keyword universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_fs_form_number:
        { an_ifc_form_number universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_fs_form_operator:
        { an_ifc_form_operator universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_fs_form_parameter:
        { an_ifc_form_parameter universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_fs_form_parenthesized:
        { an_ifc_form_parenthesized universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_fs_form_pragma:
        { an_ifc_form_pragma universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_fs_form_string:
        { an_ifc_form_string universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_fs_form_stringize:
        { an_ifc_form_stringize universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_fs_form_tuple:
        { an_ifc_form_tuple universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_fs_form_whitespace:
        { an_ifc_form_whitespace universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_form_spec_offset idx)
/*
Given the FormSpecOffset, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    an_ifc_form_spec universal;

    construct_node_prechecked(&universal, idx);
    db_node(universal);
  } else {
    fputs("Invalid FormSpec node.\n", f_debug);
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_line_offset idx)
/*
Given the LineOffset, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    an_ifc_source_line universal;

    construct_node_prechecked(&universal, idx);
    db_node(universal);
  } else {
    fputs("Invalid SourceLine node.\n", f_debug);
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_macro_index idx)
/*
Given the MacroIndex, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_ms_macro_function_like:
        { an_ifc_macro_function_like universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ms_macro_object_like:
        { an_ifc_macro_object_like universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_name_index idx)
/*
Given the NameIndex, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_ns_name_conversion:
        { an_ifc_name_conversion universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ns_name_guide:
        { an_ifc_name_guide universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ns_name_literal:
        { an_ifc_name_literal universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ns_name_operator:
        { an_ifc_name_operator universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ns_name_source_file:
        { an_ifc_name_source_file universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ns_name_specialization:
        { an_ifc_name_specialization universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ns_name_template:
        { an_ifc_name_template universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_scope_offset idx)
/*
Given the ScopeOffset, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    an_ifc_scope_descriptor universal;

    construct_node_prechecked(&universal, idx);
    db_node(universal);
  } else {
    fputs("Invalid ScopeDescriptor node.\n", f_debug);
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_stmt_index idx)
/*
Given the StmtIndex, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_ss_stmt_block:
        { an_ifc_stmt_block universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_break:
        { an_ifc_stmt_break universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_case:
        { an_ifc_stmt_case universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_continue:
        { an_ifc_stmt_continue universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_decl:
        { an_ifc_stmt_decl universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_default:
        { an_ifc_stmt_default universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_do_while:
        { an_ifc_stmt_do_while universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_empty:
        { an_ifc_stmt_empty universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_expansion:
        { an_ifc_stmt_expansion universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_expression:
        { an_ifc_stmt_expression universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_for:
        { an_ifc_stmt_for universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_goto:
        { an_ifc_stmt_goto universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_handler:
        { an_ifc_stmt_handler universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_if:
        { an_ifc_stmt_if universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_labeled:
        { an_ifc_stmt_labeled universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_return:
        { an_ifc_stmt_return universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_switch:
        { an_ifc_stmt_switch universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_try:
        { an_ifc_stmt_try universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_tuple:
        { an_ifc_stmt_tuple universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_variable_decl:
        { an_ifc_stmt_variable_decl universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_stmt_while:
        { an_ifc_stmt_while universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_syntax_index idx)
/*
Given the SyntaxIndex, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_ss_syntax_access_specifier:
        { an_ifc_syntax_access_specifier universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_alias_declaration:
        { an_ifc_syntax_alias_declaration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_alignas:
        { an_ifc_syntax_alignas universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_array_declarator:
        { an_ifc_syntax_array_declarator universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_array_index:
        { an_ifc_syntax_array_index universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_array_or_function_declarator:
        { an_ifc_syntax_array_or_function_declarator universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_asm_statement:
        { an_ifc_syntax_asm_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_attribute:
        { an_ifc_syntax_attribute universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_attribute_argument_clause:
        { an_ifc_syntax_attribute_argument_clause universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_attribute_specifier:
        { an_ifc_syntax_attribute_specifier universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_attribute_specifier_seq:
        { an_ifc_syntax_attribute_specifier_seq universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_attribute_using_prefix:
        { an_ifc_syntax_attribute_using_prefix universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_attributed_declaration:
        { an_ifc_syntax_attributed_declaration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_attributed_statement:
        { an_ifc_syntax_attributed_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_base_specifier:
        { an_ifc_syntax_base_specifier universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_base_specifier_list:
        { an_ifc_syntax_base_specifier_list universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_binary_fold_expression:
        { an_ifc_syntax_binary_fold_expression universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_break_statement:
        { an_ifc_syntax_break_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_capture_default:
        { an_ifc_syntax_capture_default universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_class_specifier:
        { an_ifc_syntax_class_specifier universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_compound_requirement:
        { an_ifc_syntax_compound_requirement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_compound_statement:
        { an_ifc_syntax_compound_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_concept_definition:
        { an_ifc_syntax_concept_definition universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_condition_declaration:
        { an_ifc_syntax_condition_declaration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_continue_statement:
        { an_ifc_syntax_continue_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_ctor_initializer:
        { an_ifc_syntax_ctor_initializer universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_decl_specifier_seq:
        { an_ifc_syntax_decl_specifier_seq universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_declaration_statement:
        { an_ifc_syntax_declaration_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_declarator:
        { an_ifc_syntax_declarator universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_decltype_specifier:
        { an_ifc_syntax_decltype_specifier universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_do_while_statement:
        { an_ifc_syntax_do_while_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_dynamic_exception_spec:
        { an_ifc_syntax_dynamic_exception_spec universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_empty_statement:
        { an_ifc_syntax_empty_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_enum_specifier:
        { an_ifc_syntax_enum_specifier universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_enumerator_definition:
        { an_ifc_syntax_enumerator_definition universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_exception_declaration:
        { an_ifc_syntax_exception_declaration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_explicit_specifier:
        { an_ifc_syntax_explicit_specifier universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_expression:
        { an_ifc_syntax_expression universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_expression_statement:
        { an_ifc_syntax_expression_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_for_range_declaration:
        { an_ifc_syntax_for_range_declaration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_for_statement:
        { an_ifc_syntax_for_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_function_body:
        { an_ifc_syntax_function_body universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_function_declarator:
        { an_ifc_syntax_function_declarator universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_function_definition:
        { an_ifc_syntax_function_definition universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_function_try_block:
        { an_ifc_syntax_function_try_block universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_goto_statement:
        { an_ifc_syntax_goto_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_handler:
        { an_ifc_syntax_handler universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_handler_seq:
        { an_ifc_syntax_handler_seq universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_if_statement:
        { an_ifc_syntax_if_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_init_capture:
        { an_ifc_syntax_init_capture universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_init_declarator:
        { an_ifc_syntax_init_declarator universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_init_statement:
        { an_ifc_syntax_init_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_labeled_statement:
        { an_ifc_syntax_labeled_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_lambda_declarator:
        { an_ifc_syntax_lambda_declarator universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_lambda_introducer:
        { an_ifc_syntax_lambda_introducer universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_mem_initializer:
        { an_ifc_syntax_mem_initializer universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_member_declaration:
        { an_ifc_syntax_member_declaration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_member_declarator:
        { an_ifc_syntax_member_declarator universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_member_function_declaration:
        { an_ifc_syntax_member_function_declaration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_member_specification:
        { an_ifc_syntax_member_specification universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_namespace_alias_definition:
        { an_ifc_syntax_namespace_alias_definition universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_nested_requirement:
        { an_ifc_syntax_nested_requirement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_new_declarator:
        { an_ifc_syntax_new_declarator universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_noexcept_specification:
        { an_ifc_syntax_noexcept_specification universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_non_type_template_argument:
        { an_ifc_syntax_non_type_template_argument universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_parameter_declarator:
        { an_ifc_syntax_parameter_declarator universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_placeholder_type_specifier:
        { an_ifc_syntax_placeholder_type_specifier universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_pointer_declarator:
        { an_ifc_syntax_pointer_declarator universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_range_based_for_statement:
        { an_ifc_syntax_range_based_for_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_requirement_body:
        { an_ifc_syntax_requirement_body universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_requires_clause:
        { an_ifc_syntax_requires_clause universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_return_statement:
        { an_ifc_syntax_return_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_seh_except:
        { an_ifc_syntax_seh_except universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_seh_finally:
        { an_ifc_syntax_seh_finally universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_seh_leave:
        { an_ifc_syntax_seh_leave universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_seh_try:
        { an_ifc_syntax_seh_try universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_simple_capture:
        { an_ifc_syntax_simple_capture universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_simple_declaration:
        { an_ifc_syntax_simple_declaration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_simple_requirement:
        { an_ifc_syntax_simple_requirement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_simple_type_specifier:
        { an_ifc_syntax_simple_type_specifier universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_statement_seq:
        { an_ifc_syntax_statement_seq universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_static_assert_declaration:
        { an_ifc_syntax_static_assert_declaration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_structured_binding_declaration:
        { an_ifc_syntax_structured_binding_declaration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_structured_binding_identifier:
        { an_ifc_syntax_structured_binding_identifier universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_super:
        { an_ifc_syntax_super universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_switch_statement:
        { an_ifc_syntax_switch_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_template_argument_list:
        { an_ifc_syntax_template_argument_list universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_template_declaration:
        { an_ifc_syntax_template_declaration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_template_id:
        { an_ifc_syntax_template_id universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_template_parameter_list:
        { an_ifc_syntax_template_parameter_list universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_template_template_parameter:
        { an_ifc_syntax_template_template_parameter universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_this_capture:
        { an_ifc_syntax_this_capture universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_trailing_return_type:
        { an_ifc_syntax_trailing_return_type universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_try_block:
        { an_ifc_syntax_try_block universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_tuple:
        { an_ifc_syntax_tuple universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_type_id:
        { an_ifc_syntax_type_id universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_type_id_list_element:
        { an_ifc_syntax_type_id_list_element universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_type_requirement:
        { an_ifc_syntax_type_requirement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_type_specifier_seq:
        { an_ifc_syntax_type_specifier_seq universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_type_template_argument:
        { an_ifc_syntax_type_template_argument universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_type_template_parameter:
        { an_ifc_syntax_type_template_parameter universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_type_trait_intrinsic:
        { an_ifc_syntax_type_trait_intrinsic universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_unary_fold_expression:
        { an_ifc_syntax_unary_fold_expression universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_using_declaration:
        { an_ifc_syntax_using_declaration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_using_declarator:
        { an_ifc_syntax_using_declarator universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_using_directive:
        { an_ifc_syntax_using_directive universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_using_enum_declaration:
        { an_ifc_syntax_using_enum_declaration universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_virtual_specifier_seq:
        { an_ifc_syntax_virtual_specifier_seq universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ss_syntax_while_statement:
        { an_ifc_syntax_while_statement universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


void db_node_at_idx(an_ifc_type_index idx)
/*
Given the TypeIndex, print a diagnostic textual representation of the
associated node.
*/
{
  if (validate(idx)) {
    switch (idx.sort) {
      case ifc_ts_type_array:
        { an_ifc_type_array universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_base:
        { an_ifc_type_base universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_decltype:
        { an_ifc_type_decltype universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_designated:
        { an_ifc_type_designated universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_expansion:
        { an_ifc_type_expansion universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_forall:
        { an_ifc_type_forall universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_function:
        { an_ifc_type_function universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_fundamental:
        { an_ifc_type_fundamental universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_lvalue_reference:
        { an_ifc_type_lvalue_reference universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_method:
        { an_ifc_type_method universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_placeholder:
        { an_ifc_type_placeholder universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_pointer:
        { an_ifc_type_pointer universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_pointer_to_member:
        { an_ifc_type_pointer_to_member universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_qualified:
        { an_ifc_type_qualified universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_rvalue_reference:
        { an_ifc_type_rvalue_reference universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_syntactic:
        { an_ifc_type_syntactic universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_syntax_tree:
        { an_ifc_type_syntax_tree universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_tor:
        { an_ifc_type_tor universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_tuple:
        { an_ifc_type_tuple universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_typename:
        { an_ifc_type_typename universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      case ifc_ts_type_unaligned:
        { an_ifc_type_unaligned universal;

          construct_node_prechecked(&universal, idx);
          db_node(universal);
        }
        break;
      default:
        fprintf(f_debug, "Node not found.");
        break;
    }  /* switch */
  } else {
    fprintf(f_debug, "Invalid %s node.\n", str_for(idx.sort));
  }  /* if */
}  /* db_node_at_idx */


/* End the suppression of GCC warnings. */
END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* DEBUG */

#endif /* !STANDALONE_UTILITY_PROGRAM */

