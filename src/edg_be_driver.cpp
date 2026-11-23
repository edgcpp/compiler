/**
 * @file edg_be_driver.cpp
 * @brief Implementation of GCC/Clang CLI driver translation, `@rspfile` expansion, and depfiles.
 * @details Normalizes arbitrary build-system invocations (`CMake`, `Meson`, `Autotools`, `Ninja`)
 * into EDG front-end arguments plus `edg_be_invocation_options_t` with zero heap allocations,
 * zero C++ exceptions, and 100% error-enum propagation.
 * @ingroup edg_be_driver
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "edg_be_driver.h"

#include <cstdio>
#include <cstring>

BEGIN_EDG_NAMESPACE

static constexpr char kCharLf = static_cast<char>(10);          ///< ASCII line-feed character (0x0A).
static constexpr char kCharCr = static_cast<char>(13);          ///< ASCII carriage-return character (0x0D).
static constexpr char kCharTab = static_cast<char>(9);          ///< ASCII horizontal tab character (0x09).
static constexpr char kCharBackslash = static_cast<char>(92);   ///< ASCII backslash character (0x5C).
static constexpr char kCharSingleQuote = static_cast<char>(39); ///< ASCII single-quote character (0x27).

/**
 * @brief Allocates and copies a substring into `cmd->arena` and returns a stable C string pointer.
 * @details Verifies `len + 1 <= cmd->arena_limit - cmd->arena_used` before copying.
 * @param[in,out] cmd Translated command arena owner.
 * @param[in] src Source character pointer.
 * @param[in] len Number of characters to copy from `src`.
 * @param[out] out_str Pointer to receive the null-terminated arena string.
 * @return `edg_be_driver_error_t::ok` on success, or `out_of_memory`.
 */
static edg_be_driver_error_t arena_dup_n(
    edg_be_translated_command_t* cmd,
    const char* src,
    size_t len,
    char** out_str) noexcept
{
  if (len + 1U > cmd->arena_limit - cmd->arena_used) {
    return edg_be_driver_error_t::out_of_memory;
  }
  char* dst = cmd->arena + cmd->arena_used;
  std::memcpy(dst, src, len);
  dst[len] = '\0';
  cmd->arena_used += len + 1U;
  *out_str = dst;
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Concatenates `prefix` and `value` into `cmd->arena` and appends to `cmd->edg_argv`.
 * @details Used to synthesize flags such as `--sys_include=/usr/include` without temporary heap buffers.
 * @param[in,out] cmd Translated command receiving the synthesized EDG argument.
 * @param[in] prefix Null-terminated option prefix.
 * @param[in] value Null-terminated option value (may be empty `""`).
 * @return `edg_be_driver_error_t::ok` on success, or `out_of_memory` if arena/argv limits are exceeded.
 */
static edg_be_driver_error_t push_edg_joined_arg(
    edg_be_translated_command_t* cmd,
    const char* prefix,
    const char* value) noexcept
{
  if (static_cast<size_t>(cmd->edg_argc) >= cmd->args_limit) {
    return edg_be_driver_error_t::out_of_memory;
  }
  const size_t plen = std::strlen(prefix);
  const size_t vlen = std::strlen(value);
  if (plen + vlen + 1U > cmd->arena_limit - cmd->arena_used) {
    return edg_be_driver_error_t::out_of_memory;
  }
  char* dst = cmd->arena + cmd->arena_used;
  std::memcpy(dst, prefix, plen);
  std::memcpy(dst + plen, value, vlen + 1U);
  cmd->arena_used += plen + vlen + 1U;
  cmd->edg_argv[cmd->edg_argc++] = dst;
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Appends a string item into a bounded option pointer list in `cmd`.
 * @details Copies `item` into `cmd->arena` and appends it to `list_buf[*io_count]`.
 * @param[in,out] cmd Translated command arena owner.
 * @param[in,out] list_buf Destination pointer array of capacity `EDG_BE_DRIVER_MAX_LIST_ITEMS`.
 * @param[in,out] io_count Current element count, incremented on success.
 * @param[in] item Null-terminated string to duplicate and store.
 * @return `edg_be_driver_error_t::ok` on success, or `out_of_memory` on capacity overflow.
 */
static edg_be_driver_error_t push_list_item(
    edg_be_translated_command_t* cmd,
    const char** list_buf,
    size_t* io_count,
    const char* item) noexcept
{
  if (*io_count >= cmd->list_limit) {
    return edg_be_driver_error_t::out_of_memory;
  }
  char* copy = nullptr;
  EDG_BE_DRIVER_CHECK(arena_dup_n(cmd, item, std::strlen(item), &copy));
  list_buf[*io_count] = copy;
  *io_count += 1U;
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Appends a single expanded token to `cmd->expanded_tokens`.
 * @details Used during recursive `@rspfile` expansion prior to option parsing.
 * @param[in,out] cmd Translated command receiving expanded raw tokens.
 * @param[in] tok_data Pointer to token characters.
 * @param[in] tok_len Length of token in bytes.
 * @return `edg_be_driver_error_t::ok` on success, or `out_of_memory` if `args_limit` is reached.
 */
static edg_be_driver_error_t push_expanded_token(
    edg_be_translated_command_t* cmd,
    const char* tok_data,
    size_t tok_len) noexcept
{
  if (cmd->num_expanded_tokens >= cmd->args_limit) {
    return edg_be_driver_error_t::out_of_memory;
  }
  char* copy = nullptr;
  EDG_BE_DRIVER_CHECK(arena_dup_n(cmd, tok_data, tok_len, &copy));
  cmd->expanded_tokens[cmd->num_expanded_tokens++] = copy;
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Recursively expands a single token or `@rspfile` into `cmd->expanded_tokens`.
 * @details Reads `@path` via `std::fopen`, tokenizes quoted/escaped arguments, skips `#` comments,
 * and recurses on nested `@inner` tokens up to `EDG_BE_DRIVER_MAX_RSP_DEPTH`.
 * @param[in] token Non-null raw CLI or response-file token.
 * @param[in] depth Current `@rspfile` nesting depth (`0` for top-level `argv`).
 * @param[in,out] cmd Translated command receiving expanded tokens.
 * @return `edg_be_driver_error_t::ok` on success, `response_file_io_error`, or `out_of_memory`.
 */
static edg_be_driver_error_t expand_one_token_or_rsp(
    const char* token,
    unsigned depth,
    edg_be_translated_command_t* cmd) noexcept
{
  if (token[0] != '@' || token[1] == '\0' ||
      std::strncmp(token, "@rpath", 6U) == 0 ||
      std::strncmp(token, "@loader_path", 12U) == 0 ||
      std::strncmp(token, "@executable_path", 16U) == 0) {
    return push_expanded_token(cmd, token, std::strlen(token));
  }
  if (depth >= EDG_BE_DRIVER_MAX_RSP_DEPTH) {
    return edg_be_driver_error_t::response_file_io_error;
  }

  FILE* fp = std::fopen(token + 1, "rb");
  if (fp == nullptr) {
    return edg_be_driver_error_t::response_file_io_error;
  }

  char file_buf[16384];
  const size_t nread = std::fread(file_buf, 1U, sizeof(file_buf) - 1U, fp);
  const int hit_eof = std::feof(fp);
  std::fclose(fp);
  if (hit_eof == 0) {
    return edg_be_driver_error_t::response_file_io_error;
  }
  file_buf[nread] = '\0';

  char tok_buf[2048];
  size_t i = 0;
  while (i < nread) {
    const char lead = file_buf[i];
    if (lead == ' ' || lead == kCharTab || lead == kCharCr || lead == kCharLf) {
      ++i;
      continue;
    }
    if (lead == '#') {
      while (i < nread && file_buf[i] != kCharLf) {
        ++i;
      }
      continue;
    }

    size_t tlen = 0;
    char quote = '\0';
    while (i < nread) {
      const char ch = file_buf[i];
      if (quote == '\0') {
        if (ch == ' ' || ch == kCharTab || ch == kCharCr || ch == kCharLf) {
          break;
        }
        if (ch == kCharSingleQuote || ch == '"') {
          quote = ch;
          ++i;
          continue;
        }
        if (ch == kCharBackslash) {
          ++i;
          if (i < nread) {
            if (tlen + 1U >= sizeof(tok_buf)) {
              return edg_be_driver_error_t::out_of_memory;
            }
            tok_buf[tlen++] = file_buf[i++];
          }
          continue;
        }
      } else {
        if (ch == quote) {
          quote = '\0';
          ++i;
          continue;
        }
        if (quote == '"' && ch == kCharBackslash) {
          ++i;
          if (i < nread) {
            if (tlen + 1U >= sizeof(tok_buf)) {
              return edg_be_driver_error_t::out_of_memory;
            }
            tok_buf[tlen++] = file_buf[i++];
          }
          continue;
        }
      }
      if (tlen + 1U >= sizeof(tok_buf)) {
        return edg_be_driver_error_t::out_of_memory;
      }
      tok_buf[tlen++] = ch;
      ++i;
    }
    if (quote != '\0') {
      return edg_be_driver_error_t::response_file_io_error;
    }
    tok_buf[tlen] = '\0';
    EDG_BE_DRIVER_CHECK(expand_one_token_or_rsp(tok_buf, depth + 1U, cmd));
  }
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Extracts the option value for flags accepting either `-X<val>`, `-X=<val>`, or `-X <val>`.
 * @details Advances `*io_index` when a separate argument token is consumed.
 * @param[in] cmd Translated command holding `expanded_tokens`.
 * @param[in] prefix_len Length of flag prefix.
 * @param[in,out] io_index Current token index in `cmd->expanded_tokens`.
 * @param[out] out_val Receives the non-empty option argument pointer.
 * @return `edg_be_driver_error_t::ok` on success, or `missing_cli_argument` if absent.
 */
static edg_be_driver_error_t consume_joined_or_next_arg(
    const edg_be_translated_command_t* cmd,
    size_t prefix_len,
    size_t* io_index,
    const char** out_val) noexcept
{
  const char* tok = cmd->expanded_tokens[*io_index];
  if (tok[prefix_len] != '\0') {
    const char* rest = tok + prefix_len;
    if (rest[0] == '=') {
      ++rest;
    }
    if (rest[0] == '\0') {
      return edg_be_driver_error_t::missing_cli_argument;
    }
    *out_val = rest;
    return edg_be_driver_error_t::ok;
  }
  if (*io_index + 1U >= cmd->num_expanded_tokens) {
    return edg_be_driver_error_t::missing_cli_argument;
  }
  *io_index += 1U;
  *out_val = cmd->expanded_tokens[*io_index];
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Maps a `-std=<dialect>` value string to EDG dialect flag and updates `cmd->options`.
 * @details Supports C89 through C23 and C++98 through C++26 (both ISO and `gnu*` dialects).
 * @param[in] std_val Value portion after `-std=`.
 * @param[in,out] cmd Translated command to update.
 * @param[out] out_edg_flag Receives the static EDG dialect flag (e.g., `"--c++20"`).
 * @return `edg_be_driver_error_t::ok` on success, or `unrecognized_cli_flag` for unknown standards.
 */
static edg_be_driver_error_t resolve_std_flag(
    const char* std_val,
    edg_be_translated_command_t* cmd,
    const char** out_edg_flag) noexcept
{
  struct StdMapEntry {
    const char* name;         ///< GCC/Clang `-std=` identifier.
    const char* edg_flag;     ///< Corresponding EDG dialect option.
    bool is_cxx;              ///< True for C++ standards, false for C standards.
    bool is_gnu;              ///< True when GNU extensions are enabled.
    uint32_t std_version_num; ///< ISO year/month code.
  };

  static const StdMapEntry kTable[] = {
    {"c89", "--c89", false, false, 198900U},
    {"c90", "--c89", false, false, 198900U},
    {"iso9899:1990", "--c89", false, false, 198900U},
    {"gnu89", "--c89", false, true, 198900U},
    {"gnu90", "--c89", false, true, 198900U},
    {"c99", "--c99", false, false, 199901U},
    {"gnu99", "--c99", false, true, 199901U},
    {"c11", "--c11", false, false, 201112U},
    {"gnu11", "--c11", false, true, 201112U},
    {"c17", "--c17", false, false, 201710U},
    {"c18", "--c17", false, false, 201710U},
    {"gnu17", "--c17", false, true, 201710U},
    {"gnu18", "--c17", false, true, 201710U},
    {"c2x", "--c23", false, false, 202311U},
    {"c23", "--c23", false, false, 202311U},
    {"gnu2x", "--c23", false, true, 202311U},
    {"gnu23", "--c23", false, true, 202311U},
    {"c++98", "--c++03", true, false, 199711U},
    {"c++03", "--c++03", true, false, 199711U},
    {"gnu++98", "--c++03", true, true, 199711U},
    {"gnu++03", "--c++03", true, true, 199711U},
    {"c++11", "--c++11", true, false, 201103U},
    {"gnu++11", "--c++11", true, true, 201103U},
    {"c++14", "--c++14", true, false, 201402U},
    {"gnu++14", "--c++14", true, true, 201402U},
    {"c++17", "--c++17", true, false, 201703U},
    {"gnu++17", "--c++17", true, true, 201703U},
    {"c++2a", "--c++20", true, false, 202002U},
    {"c++20", "--c++20", true, false, 202002U},
    {"gnu++2a", "--c++20", true, true, 202002U},
    {"gnu++20", "--c++20", true, true, 202002U},
    {"c++2b", "--c++23", true, false, 202302U},
    {"c++23", "--c++23", true, false, 202302U},
    {"gnu++2b", "--c++23", true, true, 202302U},
    {"gnu++23", "--c++23", true, true, 202302U},
    {"c++2c", "--c++26", true, false, 202600U},
    {"c++26", "--c++26", true, false, 202600U},
    {"gnu++2c", "--c++26", true, true, 202600U},
    {"gnu++26", "--c++26", true, true, 202600U}
  };

  for (size_t i = 0; i < sizeof(kTable) / sizeof(kTable[0]); ++i) {
    if (std::strcmp(std_val, kTable[i].name) == 0) {
      cmd->options.language_is_cplusplus = kTable[i].is_cxx;
      cmd->options.gnu_extensions = kTable[i].is_gnu;
      cmd->options.language_std_version = kTable[i].std_version_num;
      *out_edg_flag = kTable[i].edg_flag;
      return edg_be_driver_error_t::ok;
    }
  }
  return edg_be_driver_error_t::unrecognized_cli_flag;
}

/**
 * @brief Splits a comma-separated `-Wl,<arg1>,<arg2>` string into `linker_passthrough_args`.
 * @details Also extracts `-rpath` values into `cmd->options.rpath_dirs` when present.
 * @param[in] csv Comma-separated payload following `"-Wl,"`.
 * @param[in,out] cmd Translated command receiving linker passthrough arguments.
 * @return `edg_be_driver_error_t::ok` on success, `missing_cli_argument` if empty, or `out_of_memory`.
 */
static edg_be_driver_error_t parse_wl_passthrough(
    const char* csv,
    edg_be_translated_command_t* cmd) noexcept
{
  if (csv[0] == '\0') {
    return edg_be_driver_error_t::missing_cli_argument;
  }
  const char* cur = csv;
  bool expect_rpath = false;
  for (;;) {
    const char* comma = std::strchr(cur, ',');
    const size_t seg_len = (comma != nullptr) ? static_cast<size_t>(comma - cur) : std::strlen(cur);
    char* item = nullptr;
    EDG_BE_DRIVER_CHECK(arena_dup_n(cmd, cur, seg_len, &item));
    EDG_BE_DRIVER_CHECK(push_list_item(
        cmd, cmd->linker_args_buf, &cmd->options.num_linker_passthrough_args, item));
    if (expect_rpath) {
      EDG_BE_DRIVER_CHECK(push_list_item(cmd, cmd->rpaths_buf, &cmd->options.num_rpath_dirs, item));
      expect_rpath = false;
    } else if (std::strcmp(item, "-rpath") == 0) {
      expect_rpath = true;
    } else if (std::strncmp(item, "-rpath=", 7U) == 0 && item[7] != '\0') {
      EDG_BE_DRIVER_CHECK(push_list_item(cmd, cmd->rpaths_buf, &cmd->options.num_rpath_dirs, item + 7));
    }
    if (comma == nullptr) {
      break;
    }
    cur = comma + 1;
  }
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Classifies an input file path by extension as a linker object/archive or C/C++ source.
 * @details Recognizes `.o`, `.obj`, `.a`, `.so`, `.dylib`, and `.lib` as linker inputs,
 * and infers C++ mode on `.cc`, `.cp`, `.cxx`, `.cpp`, `.CPP`, `.c++`, `.C`, `.ii`, `.ixx`, `.cppm`.
 * @param[in] path Input positional file path.
 * @param[in] forced_lang Active `-x <lang>` override (`""` when `"none"`).
 * @param[out] out_is_object Receives `true` if `path` is a precompiled object/library input.
 * @param[out] out_is_cxx Receives `true` if `path` (or `forced_lang`) indicates C++.
 * @return `edg_be_driver_error_t::ok`.
 */
static edg_be_driver_error_t classify_positional_input(
    const char* path,
    const char* forced_lang,
    bool* out_is_object,
    bool* out_is_cxx) noexcept
{
  if (path[0] == '\0') {
    return edg_be_driver_error_t::invalid_argument;
  }
  *out_is_object = false;
  *out_is_cxx = false;
  if (forced_lang[0] != '\0') {
    *out_is_cxx = (std::strncmp(forced_lang, "c++", 3U) == 0);
    return edg_be_driver_error_t::ok;
  }

  const char* base = path;
  for (const char* p = path; *p != '\0'; ++p) {
    if (*p == '/' || *p == kCharBackslash) {
      base = p + 1;
    }
  }
  const char* dot = std::strrchr(base, '.');
  if (dot == nullptr) {
    return edg_be_driver_error_t::ok;
  }

  static const char* const kObjExts[] = {".o", ".obj", ".a", ".so", ".dylib", ".lib"};
  for (size_t i = 0; i < sizeof(kObjExts) / sizeof(kObjExts[0]); ++i) {
    if (std::strcmp(dot, kObjExts[i]) == 0) {
      *out_is_object = true;
      return edg_be_driver_error_t::ok;
    }
  }

  static const char* const kCxxExts[] = {
    ".cpp", ".cc", ".cxx", ".cp", ".CPP", ".c++", ".C", ".ii", ".ixx", ".cppm"
  };
  for (size_t i = 0; i < sizeof(kCxxExts) / sizeof(kCxxExts[0]); ++i) {
    if (std::strcmp(dot, kCxxExts[i]) == 0) {
      *out_is_cxx = true;
      break;
    }
  }
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Parses `-fsanitize=<list>` comma-separated sanitizer names onto `opts`.
 * @details Sets `sanitize_address`, `sanitize_undefined`, `sanitize_thread`, and `sanitize_memory`.
 * @param[in] list Comma-separated sanitizer list string.
 * @param[in,out] opts Invocation options structure to update.
 * @return `edg_be_driver_error_t::ok` on success, or `missing_cli_argument` if empty.
 */
static edg_be_driver_error_t apply_sanitize_list(
    const char* list,
    edg_be_invocation_options_t* opts) noexcept
{
  if (list[0] == '\0') {
    return edg_be_driver_error_t::missing_cli_argument;
  }
  const char* p = list;
  for (;;) {
    const char* c = std::strchr(p, ',');
    const size_t n = (c != nullptr) ? static_cast<size_t>(c - p) : std::strlen(p);
    char name[32] = "";
    if (n < sizeof(name)) {
      std::memcpy(name, p, n);
      name[n] = '\0';
    }
    if (std::strcmp(name, "address") == 0) {
      opts->sanitize_address = true;
    } else if (std::strcmp(name, "undefined") == 0) {
      opts->sanitize_undefined = true;
    } else if (std::strcmp(name, "thread") == 0) {
      opts->sanitize_thread = true;
    } else if (std::strcmp(name, "memory") == 0) {
      opts->sanitize_memory = true;
    }
    if (c == nullptr) {
      break;
    }
    p = c + 1;
  }
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Appends raw characters into a bounded depfile buffer.
 * @details Checks `buf_cap` and advances `*io_pos`.
 * @param[out] out_buf Destination buffer.
 * @param[in] buf_cap Total capacity of `out_buf`.
 * @param[in,out] io_pos Current write offset in `out_buf`.
 * @param[in] src Characters to append.
 * @param[in] len Number of characters to append.
 * @return `edg_be_driver_error_t::ok` on success, or `out_of_memory` if `out_buf` would overflow.
 */
static edg_be_driver_error_t depbuf_append_raw(
    char* out_buf,
    size_t buf_cap,
    size_t* io_pos,
    const char* src,
    size_t len) noexcept
{
  if (len + 1U > buf_cap - *io_pos) {
    return edg_be_driver_error_t::out_of_memory;
  }
  std::memcpy(out_buf + *io_pos, src, len);
  *io_pos += len;
  out_buf[*io_pos] = '\0';
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Escapes and appends a Make target or prerequisite token into `out_buf`.
 * @details Escapes spaces (`\ `), `#` (`\#`), and `$` (`$$`) unless `already_quoted` (`-MQ`).
 * @param[out] out_buf Destination buffer.
 * @param[in] buf_cap Capacity of `out_buf`.
 * @param[in,out] io_pos Current write offset in `out_buf`.
 * @param[in] raw Null-terminated path string to write.
 * @param[in] already_quoted When true (`-MQ`), appends `raw` verbatim without escaping.
 * @return `edg_be_driver_error_t::ok` on success, or `out_of_memory`.
 */
static edg_be_driver_error_t depbuf_append_escaped(
    char* out_buf,
    size_t buf_cap,
    size_t* io_pos,
    const char* raw,
    bool already_quoted) noexcept
{
  if (already_quoted) {
    return depbuf_append_raw(out_buf, buf_cap, io_pos, raw, std::strlen(raw));
  }
  for (size_t i = 0; raw[i] != '\0'; ++i) {
    const char ch = raw[i];
    if (ch == ' ' || ch == '#') {
      const char esc[2] = {kCharBackslash, ch};
      EDG_BE_DRIVER_CHECK(depbuf_append_raw(out_buf, buf_cap, io_pos, esc, 2U));
    } else if (ch == '$') {
      EDG_BE_DRIVER_CHECK(depbuf_append_raw(out_buf, buf_cap, io_pos, "$$", 2U));
    } else {
      EDG_BE_DRIVER_CHECK(depbuf_append_raw(out_buf, buf_cap, io_pos, &ch, 1U));
    }
  }
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Converts an `edg_be_driver_error_t` status code to a static human-readable string.
 * @details Covers all 17 error enumerators plus a fallback for out-of-range casts.
 * @param[in] err Error code enumerator to convert.
 * @param[out] out_str Pointer to receive the static null-terminated description string.
 * @return `edg_be_driver_error_t::ok` on success, or `invalid_argument` if `out_str` is null.
 */
edg_be_driver_error_t edg_be_driver_error_to_string(
    edg_be_driver_error_t err,
    const char** out_str) noexcept
{
  EDG_BE_DRIVER_CHECK_NOT_NULL(out_str);
  switch (err) {
    case edg_be_driver_error_t::ok:
      *out_str = "ok";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::invalid_argument:
      *out_str = "invalid argument";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::out_of_memory:
      *out_str = "out of memory";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::unrecognized_cli_flag:
      *out_str = "unrecognized command-line flag";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::missing_cli_argument:
      *out_str = "missing argument to command-line flag";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::conflicting_backend_flags:
      *out_str = "conflicting backend selection flags";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::backend_not_compiled_in:
      *out_str = "requested backend not compiled in";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::sdk_discovery_failed:
      *out_str = "host SDK or system header discovery failed";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::response_file_io_error:
      *out_str = "response file I/O or syntax error";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::depfile_write_failed:
      *out_str = "failed to write Make dependency file";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::temp_file_creation_failed:
      *out_str = "failed to create temporary artifact path";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::frontend_compilation_failed:
      *out_str = "front-end compilation failed";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::llvm_backend_failed:
      *out_str = "LLVM backend failed";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::gcc_backend_failed:
      *out_str = "GCC backend failed";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::c_backend_failed:
      *out_str = "C-generating backend failed";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::linker_execution_failed:
      *out_str = "linker execution failed";
      return edg_be_driver_error_t::ok;
    case edg_be_driver_error_t::io_error:
      *out_str = "I/O error";
      return edg_be_driver_error_t::ok;
    default:
      *out_str = "unknown driver error";
      return edg_be_driver_error_t::ok;
  }
}

/**
 * @brief Initializes an `edg_be_invocation_options_t` structure to safe deterministic defaults.
 * @details Sets `debug_level = 0`, `opt_level = 0`, `exceptions_enabled = true`, and `rtti_enabled = true`.
 * @param[out] out_opts Pointer to the invocation options structure to initialize.
 * @return `edg_be_driver_error_t::ok` on success, or `invalid_argument` if `out_opts` is null.
 */
edg_be_driver_error_t edg_be_invocation_options_init(
    edg_be_invocation_options_t* out_opts) noexcept
{
  EDG_BE_DRIVER_CHECK_NOT_NULL(out_opts);
  std::memset(out_opts, 0, sizeof(*out_opts));
  out_opts->selected_backend = edg_backend_kind_t::auto_select;
  out_opts->artifact_kind = edg_output_artifact_kind_t::executable;
  out_opts->reloc_mode = edg_reloc_mode_t::default_reloc;
  out_opts->lto_mode = edg_lto_mode_t::none;
  out_opts->symbol_visibility = edg_visibility_mode_t::default_vis;
  out_opts->query_kind = edg_driver_query_kind_t::none;
  out_opts->exceptions_enabled = true;
  out_opts->rtti_enabled = true;
  out_opts->gnu_extensions = true;
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Initializes an `edg_be_translated_command_t` structure to empty defaults.
 * @details Wires all `options.*` array pointers to the inline `*_buf` storage in `out_cmd`.
 * @param[out] out_cmd Pointer to the translated command container to initialize.
 * @return `edg_be_driver_error_t::ok` on success, or `invalid_argument` if `out_cmd` is null.
 */
edg_be_driver_error_t edg_be_translated_command_init(
    edg_be_translated_command_t* out_cmd) noexcept
{
  EDG_BE_DRIVER_CHECK_NOT_NULL(out_cmd);
  std::memset(out_cmd, 0, sizeof(*out_cmd));
  out_cmd->options.exceptions_enabled = true;
  out_cmd->options.rtti_enabled = true;
  out_cmd->options.gnu_extensions = true;
  out_cmd->arena_limit = EDG_BE_DRIVER_ARENA_CAPACITY;
  out_cmd->args_limit = EDG_BE_DRIVER_MAX_ARGS;
  out_cmd->list_limit = EDG_BE_DRIVER_MAX_LIST_ITEMS;
  out_cmd->options.dep_targets = out_cmd->dep_targets_buf;
  out_cmd->options.input_source_files = out_cmd->sources_buf;
  out_cmd->options.input_object_files = out_cmd->objects_buf;
  out_cmd->options.library_search_dirs = out_cmd->lib_dirs_buf;
  out_cmd->options.libraries = out_cmd->libs_buf;
  out_cmd->options.linker_passthrough_args = out_cmd->linker_args_buf;
  out_cmd->options.rpath_dirs = out_cmd->rpaths_buf;
  out_cmd->options.extra_backend_args = out_cmd->extra_be_buf;
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Expands `@rspfile` response-file arguments into `cmd->expanded_tokens`.
 * @details Preserves `argv[0]` verbatim as the program name and expands `argv[1..argc-1]`.
 * @param[in] argc Argument count of raw `argv` (must be >= 1).
 * @param[in] argv Raw command-line argument array.
 * @param[in,out] cmd Initialized translated command structure receiving `expanded_tokens`.
 * @return `edg_be_driver_error_t::ok` on success, `invalid_argument`, `response_file_io_error`, or `out_of_memory`.
 */
edg_be_driver_error_t edg_be_expand_response_files(
    int argc,
    const char* const* argv,
    edg_be_translated_command_t* cmd) noexcept
{
  EDG_BE_DRIVER_CHECK_NOT_NULL(argv);
  EDG_BE_DRIVER_CHECK_NOT_NULL(cmd);
  if (argc < 1 || argv[0] == nullptr) {
    return edg_be_driver_error_t::invalid_argument;
  }
  cmd->num_expanded_tokens = 0U;
  EDG_BE_DRIVER_CHECK(push_expanded_token(cmd, argv[0], std::strlen(argv[0])));
  for (int i = 1; i < argc; ++i) {
    EDG_BE_DRIVER_CHECK_NOT_NULL(argv[i]);
    EDG_BE_DRIVER_CHECK(expand_one_token_or_rsp(argv[i], 0U, cmd));
  }
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Translates a GCC/Clang or EDG command line into `edg_argv` and `edg_be_invocation_options_t`.
 * @details Delegates to `edg_be_translate_command_line_bounded` with full default capacities.
 * @param[in] argc Raw command-line argument count.
 * @param[in] argv Raw command-line argument vector.
 * @param[in] sdk_env Optional SDK probe environment (nullptr skips auto-SDK injection).
 * @param[out] out_cmd Pointer to receive the translated EDG argv and normalized options.
 * @return `edg_be_driver_error_t::ok` on success, or an appropriate `edg_be_driver_error_t`.
 */
edg_be_driver_error_t edg_be_translate_command_line(
    int argc,
    const char* const* argv,
    const edg_be_sdk_probe_env_t* sdk_env,
    edg_be_translated_command_t* out_cmd) noexcept
{
  return edg_be_translate_command_line_bounded(argc, argv, sdk_env, 0U, 0U, 0U, out_cmd);
}

/**
 * @brief Translates a command line using explicit arena, argument, and list capacity caps.
 * @details Zero-initializes `out_cmd` first, applies any non-zero capacity caps, expands `@rspfile`
 * inputs, normalizes GCC/Clang and EDG flags, and injects discovered SDK paths.
 * @param[in] argc Raw command-line argument count.
 * @param[in] argv Raw command-line argument vector.
 * @param[in] sdk_env Optional SDK probe environment (nullptr skips auto-SDK injection).
 * @param[in] arena_limit Maximum arena bytes (`0` uses `EDG_BE_DRIVER_ARENA_CAPACITY`).
 * @param[in] args_limit Maximum token/argv slots (`0` uses `EDG_BE_DRIVER_MAX_ARGS`).
 * @param[in] list_limit Maximum entries per option list (`0` uses `EDG_BE_DRIVER_MAX_LIST_ITEMS`).
 * @param[out] out_cmd Pointer to receive the translated EDG argv and normalized options.
 * @return `edg_be_driver_error_t::ok` on success, or an appropriate `edg_be_driver_error_t`.
 */
edg_be_driver_error_t edg_be_translate_command_line_bounded(
    int argc,
    const char* const* argv,
    const edg_be_sdk_probe_env_t* sdk_env,
    size_t arena_limit,
    size_t args_limit,
    size_t list_limit,
    edg_be_translated_command_t* out_cmd) noexcept
{
  EDG_BE_DRIVER_CHECK(edg_be_translated_command_init(out_cmd));
  if (arena_limit > 0U && arena_limit < EDG_BE_DRIVER_ARENA_CAPACITY) {
    out_cmd->arena_limit = arena_limit;
  }
  if (args_limit > 0U && args_limit < EDG_BE_DRIVER_MAX_ARGS) {
    out_cmd->args_limit = args_limit;
  }
  if (list_limit > 0U && list_limit < EDG_BE_DRIVER_MAX_LIST_ITEMS) {
    out_cmd->list_limit = list_limit;
  }

  EDG_BE_DRIVER_CHECK(edg_be_expand_response_files(argc, argv, out_cmd));

  edg_be_invocation_options_t* opts = &out_cmd->options;
  const char* prog = out_cmd->expanded_tokens[0];
  const size_t prog_len = std::strlen(prog);
  if (prog_len >= 2U && std::strcmp(prog + prog_len - 2U, "++") == 0) {
    opts->language_is_cplusplus = true;
  }

  EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, prog, ""));

  char forced_lang[32] = "";
  bool saw_explicit_std_or_mode = false;
  bool saw_explicit_emulation_flag = false;
  bool saw_verbose_flag = false;
  bool saw_modules_flag = false;

  for (size_t i = 1U; i < out_cmd->num_expanded_tokens; ++i) {
    const char* tok = out_cmd->expanded_tokens[i];
    const char* emit_prefix = nullptr;
    const char* emit_val = "";

    if (tok[0] != '-' || tok[1] == '\0') {
      bool is_obj = false;
      bool is_cxx = false;
      EDG_BE_DRIVER_CHECK(classify_positional_input(tok, forced_lang, &is_obj, &is_cxx));
      if (is_obj) {
        EDG_BE_DRIVER_CHECK(push_list_item(out_cmd, out_cmd->objects_buf, &opts->num_input_object_files, tok));
      } else {
        if (is_cxx) {
          opts->language_is_cplusplus = true;
        }
        EDG_BE_DRIVER_CHECK(push_list_item(out_cmd, out_cmd->sources_buf, &opts->num_input_source_files, tok));
      }
      continue;
    }

    /* Introspection queries */
    if (std::strcmp(tok, "--version") == 0) {
      opts->query_kind = edg_driver_query_kind_t::print_version;
    } else if (std::strcmp(tok, "-v") == 0) {
      saw_verbose_flag = true;
    } else if (std::strcmp(tok, "-dumpmachine") == 0 || std::strcmp(tok, "--dumpmachine") == 0) {
      opts->query_kind = edg_driver_query_kind_t::dump_machine;
    } else if (std::strcmp(tok, "-dumpversion") == 0 || std::strcmp(tok, "--dumpversion") == 0) {
      opts->query_kind = edg_driver_query_kind_t::dump_version;
    } else if (std::strcmp(tok, "-dumpfullversion") == 0 || std::strcmp(tok, "--dumpfullversion") == 0) {
      opts->query_kind = edg_driver_query_kind_t::dump_full_version;
    } else if (std::strcmp(tok, "-print-search-dirs") == 0 || std::strcmp(tok, "--print-search-dirs") == 0) {
      opts->query_kind = edg_driver_query_kind_t::print_search_dirs;
    } else if (std::strncmp(tok, "-print-prog-name=", 17U) == 0) {
      opts->query_kind = edg_driver_query_kind_t::print_prog_name;
      opts->query_argument = tok + 17U;
    } else if (std::strncmp(tok, "-print-file-name=", 17U) == 0) {
      opts->query_kind = edg_driver_query_kind_t::print_file_name;
      opts->query_argument = tok + 17U;
    } else if (std::strcmp(tok, "-print-libgcc-file-name") == 0) {
      opts->query_kind = edg_driver_query_kind_t::print_libgcc_file_name;
    } else if (std::strcmp(tok, "-print-resource-dir") == 0) {
      opts->query_kind = edg_driver_query_kind_t::print_resource_dir;
    } else if (std::strncmp(tok, "--backend", 9U) == 0 && (tok[9] == '\0' || tok[9] == '=')) {
      const char* bval = nullptr;
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 9U, &i, &bval));
      if (std::strcmp(bval, "llvm") == 0) {
        opts->selected_backend = edg_backend_kind_t::llvm_be;
        emit_prefix = "--backend=llvm";
      } else if (std::strcmp(bval, "gcc") == 0) {
        opts->selected_backend = edg_backend_kind_t::gcc_be;
        emit_prefix = "--backend=gcc";
      } else if (std::strcmp(bval, "c") == 0) {
        opts->selected_backend = edg_backend_kind_t::c_gen_be;
        emit_prefix = "--backend=c";
      } else {
        return edg_be_driver_error_t::conflicting_backend_flags;
      }
    } else if (std::strcmp(tok, "--llvm-be") == 0 || std::strcmp(tok, "--llvm_gen_be") == 0) {
      if (opts->selected_backend == edg_backend_kind_t::gcc_be) {
        return edg_be_driver_error_t::conflicting_backend_flags;
      }
      opts->selected_backend = edg_backend_kind_t::llvm_be;
      emit_prefix = tok;
    } else if (std::strcmp(tok, "--gcc-be") == 0 || std::strcmp(tok, "--gcc_gen_be") == 0) {
      if (opts->selected_backend == edg_backend_kind_t::llvm_be) {
        return edg_be_driver_error_t::conflicting_backend_flags;
      }
      opts->selected_backend = edg_backend_kind_t::gcc_be;
      emit_prefix = tok;
    } else if (std::strcmp(tok, "--c-be") == 0 || std::strcmp(tok, "--c_gen_be") == 0) {
      opts->selected_backend = edg_backend_kind_t::c_gen_be;
      emit_prefix = tok;
    } else if (std::strcmp(tok, "--fe_only") == 0) {
      opts->selected_backend = edg_backend_kind_t::fe_only;
      opts->artifact_kind = edg_output_artifact_kind_t::syntax_only;
      emit_prefix = tok;
    } else if (std::strncmp(tok, "--sysroot", 9U) == 0 && (tok[9] == '\0' || tok[9] == '=')) {
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 9U, &i, &opts->sysroot));
    } else if (std::strncmp(tok, "--target=", 9U) == 0) {
      if (tok[9] == '\0') {
        return edg_be_driver_error_t::missing_cli_argument;
      }
      opts->target_triple = tok + 9U;
    } else if (tok[1] == '-') {
      if (std::strncmp(tok, "--c++", 5U) == 0) {
        opts->language_is_cplusplus = true;
        saw_explicit_std_or_mode = true;
      } else if (std::strcmp(tok, "--c") == 0 || std::strcmp(tok, "--c89") == 0 ||
                 std::strcmp(tok, "--c99") == 0 || std::strcmp(tok, "--c11") == 0 ||
                 std::strcmp(tok, "--c17") == 0 || std::strcmp(tok, "--c23") == 0) {
        opts->language_is_cplusplus = false;
        saw_explicit_std_or_mode = true;
      }
      if (std::strcmp(tok, "--clang") == 0 || std::strcmp(tok, "--gcc") == 0 ||
          std::strcmp(tok, "--g++") == 0 || std::strcmp(tok, "--microsoft") == 0) {
        saw_explicit_emulation_flag = true;
      }
      if (std::strncmp(tok, "--clear_flag=use_predefined_macro_file", 38U) == 0 ||
          std::strncmp(tok, "--edg_base_dir", 14U) == 0 ||
          std::strcmp(tok, "--dump_configuration") == 0) {
        out_cmd->native_edg_mode = true;
      }
      if (std::strncmp(tok, "--gen_llvm_file_name", 20U) == 0) {
        opts->emit_llvm_ir_file = true;
      }
      static const char* const kTwoTokenLongOpts[] = {
        "--edg_base_dir",
        "--gen_llvm_file_name",
        "--gen_llvm_bc_file_name",
        "--gen_obj_file_name",
        "--gen_asm_file_name",
        "--gen_c_file_name",
        "--fsanitize",
        "--llvm_target",
        "--fprofile-use",
        "--flto",
        "--gsplit-dwarf",
        "--mcpu",
        "--march",
        "--mattr",
        "--mcmodel",
        "--linker_arg",
        "--sys_include",
        "--preinclude",
        "--preinclude_macros"
      };
      bool takes_next_arg = false;
      for (size_t k = 0U; k < sizeof(kTwoTokenLongOpts) / sizeof(kTwoTokenLongOpts[0]); ++k) {
        if (std::strcmp(tok, kTwoTokenLongOpts[k]) == 0) {
          takes_next_arg = true;
          break;
        }
      }
      if (takes_next_arg) {
        const char* next_val = nullptr;
        EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, std::strlen(tok), &i, &next_val));
        EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, tok, ""));
        emit_prefix = next_val;
      } else {
        emit_prefix = tok;
      }
    } else if (std::strcmp(tok, "-c") == 0) {
      opts->artifact_kind = edg_output_artifact_kind_t::object_file;
    } else if (std::strcmp(tok, "-S") == 0) {
      opts->artifact_kind = edg_output_artifact_kind_t::assembly_file;
    } else if (std::strcmp(tok, "-E") == 0) {
      opts->artifact_kind = edg_output_artifact_kind_t::preprocessed_output;
      opts->selected_backend = edg_backend_kind_t::fe_only;
      emit_prefix = "-E";
    } else if (std::strcmp(tok, "-fsyntax-only") == 0) {
      opts->artifact_kind = edg_output_artifact_kind_t::syntax_only;
      opts->selected_backend = edg_backend_kind_t::fe_only;
      emit_prefix = "--fe_only";
    } else if (std::strcmp(tok, "-emit-llvm") == 0) {
      opts->emit_llvm_ir_file = true;
      if (opts->artifact_kind == edg_output_artifact_kind_t::executable) {
        opts->artifact_kind = edg_output_artifact_kind_t::ir_or_c_file;
      }
    } else if (std::strcmp(tok, "-shared") == 0 || std::strcmp(tok, "-dynamiclib") == 0) {
      opts->artifact_kind = edg_output_artifact_kind_t::shared_library;
      if (opts->reloc_mode == edg_reloc_mode_t::default_reloc) {
        opts->reloc_mode = edg_reloc_mode_t::pic;
      }
    } else if (std::strcmp(tok, "-static") == 0) {
      opts->static_link = true;
    } else if (std::strcmp(tok, "-pie") == 0) {
      opts->reloc_mode = edg_reloc_mode_t::pie;
    } else if (std::strcmp(tok, "-no-pie") == 0) {
      opts->reloc_mode = edg_reloc_mode_t::static_no_pic;
    } else if (std::strcmp(tok, "-pipe") == 0 || std::strcmp(tok, "-Qunused-arguments") == 0 ||
               std::strcmp(tok, "-P") == 0 || std::strcmp(tok, "-C") == 0 ||
               std::strcmp(tok, "-CC") == 0 || std::strcmp(tok, "-dM") == 0 ||
               std::strncmp(tok, "-stdlib=", 8U) == 0) {
      /* Accepted as compatibility flags. */
    } else if (std::strcmp(tok, "-ansi") == 0) {
      saw_explicit_std_or_mode = true;
      opts->language_is_cplusplus = false;
      opts->gnu_extensions = false;
      opts->language_std_version = 198900U;
      emit_prefix = "--c89";
    } else if (std::strcmp(tok, "-arch") == 0 || std::strncmp(tok, "-arch=", 6U) == 0) {
      const char* aval = nullptr;
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 5U, &i, &aval));
    } else if (tok[1] == 'o') {
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 2U, &i, &opts->output_file_path));
    } else if (std::strncmp(tok, "-std=", 5U) == 0) {
      saw_explicit_std_or_mode = true;
      EDG_BE_DRIVER_CHECK(resolve_std_flag(tok + 5U, out_cmd, &emit_prefix));
    } else if (tok[1] == 'x') {
      const char* lang = nullptr;
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 2U, &i, &lang));
      if (std::strcmp(lang, "none") == 0) {
        forced_lang[0] = '\0';
      } else if (std::strcmp(lang, "c") == 0 || std::strcmp(lang, "c-header") == 0 ||
                 std::strcmp(lang, "cpp-output") == 0 || std::strcmp(lang, "assembler-with-cpp") == 0 ||
                 std::strcmp(lang, "c++") == 0 || std::strcmp(lang, "c++-header") == 0 ||
                 std::strcmp(lang, "c++-cpp-output") == 0) {
        std::snprintf(forced_lang, sizeof(forced_lang), "%s", lang);
        opts->language_is_cplusplus = (std::strncmp(lang, "c++", 3U) == 0);
      } else {
        return edg_be_driver_error_t::unrecognized_cli_flag;
      }
    } else if (tok[1] == 'I') {
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 2U, &i, &emit_val));
      emit_prefix = "-I";
    } else if (std::strncmp(tok, "-isystem", 8U) == 0) {
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 8U, &i, &emit_val));
      emit_prefix = "--sys_include=";
    } else if (std::strncmp(tok, "-iquote", 7U) == 0) {
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 7U, &i, &emit_val));
      emit_prefix = "-I";
    } else if (std::strncmp(tok, "-idirafter", 10U) == 0) {
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 10U, &i, &emit_val));
      emit_prefix = "--sys_include=";
    } else if (std::strcmp(tok, "-isysroot") == 0) {
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 9U, &i, &opts->sysroot));
    } else if (std::strcmp(tok, "-resource-dir") == 0 || std::strncmp(tok, "-resource-dir=", 14U) == 0) {
      const char* rdir = nullptr;
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 13U, &i, &rdir));
    } else if (std::strcmp(tok, "-iprefix") == 0 || std::strcmp(tok, "-iwithprefix") == 0) {
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, std::strlen(tok), &i, &emit_val));
      emit_prefix = "--sys_include=";
    } else if (std::strcmp(tok, "-nostdinc") == 0) {
      opts->nostdinc = true;
    } else if (std::strcmp(tok, "-nostdinc++") == 0) {
      opts->nostdincxx = true;
    } else if (std::strcmp(tok, "-nobuiltininc") == 0) {
      opts->nobuiltininc = true;
    } else if (tok[1] == 'D' || tok[1] == 'U') {
      emit_prefix = (tok[1] == 'D') ? "-D" : "-U";
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 2U, &i, &emit_val));
    } else if (std::strcmp(tok, "-include") == 0 || std::strcmp(tok, "-imacros") == 0) {
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, std::strlen(tok), &i, &emit_val));
      emit_prefix = "--preinclude=";
    } else if (std::strcmp(tok, "-M") == 0 || std::strcmp(tok, "-MM") == 0) {
      opts->generate_depfile = true;
      opts->dep_system_headers = (tok[2] == '\0');
      opts->artifact_kind = edg_output_artifact_kind_t::preprocessed_output;
      opts->selected_backend = edg_backend_kind_t::fe_only;
    } else if (std::strcmp(tok, "-MD") == 0 || std::strcmp(tok, "-MMD") == 0) {
      opts->generate_depfile = true;
      opts->dep_system_headers = (tok[2] == 'D');
    } else if (std::strcmp(tok, "-MP") == 0) {
      opts->dep_phony_targets = true;
    } else if (std::strcmp(tok, "-MG") == 0) {
      /* Accepted for Make dependency compatibility. */
    } else if (std::strncmp(tok, "-MF", 3U) == 0) {
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 3U, &i, &opts->dep_file_path));
    } else if (std::strncmp(tok, "-MT", 3U) == 0 || std::strncmp(tok, "-MQ", 3U) == 0) {
      const bool is_mq = (tok[2] == 'Q');
      const char* mt = nullptr;
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 3U, &i, &mt));
      const size_t idx = opts->num_dep_targets;
      EDG_BE_DRIVER_CHECK(push_list_item(out_cmd, out_cmd->dep_targets_buf, &opts->num_dep_targets, mt));
      out_cmd->dep_target_is_quoted[idx] = is_mq;
    } else if (tok[1] == 'O') {
      const char* o = tok + 2;
      if (o[0] == '\0' || std::strcmp(o, "1") == 0 || std::strcmp(o, "g") == 0) {
        opts->opt_level = 1U;
        opts->opt_size_level = 0U;
      } else if (std::strcmp(o, "0") == 0) {
        opts->opt_level = 0U;
        opts->opt_size_level = 0U;
      } else if (std::strcmp(o, "2") == 0) {
        opts->opt_level = 2U;
        opts->opt_size_level = 0U;
      } else if (std::strcmp(o, "3") == 0) {
        opts->opt_level = 3U;
        opts->opt_size_level = 0U;
      } else if (std::strcmp(o, "s") == 0) {
        opts->opt_level = 2U;
        opts->opt_size_level = 1U;
      } else if (std::strcmp(o, "z") == 0) {
        opts->opt_level = 2U;
        opts->opt_size_level = 2U;
      } else if (std::strcmp(o, "fast") == 0) {
        opts->opt_level = 3U;
        opts->opt_size_level = 0U;
        opts->fast_math = true;
      } else {
        return edg_be_driver_error_t::unrecognized_cli_flag;
      }
    } else if (std::strcmp(tok, "-gcodeview") == 0) {
      opts->codeview_debug = true;
      if (opts->debug_level == 0U) {
        opts->debug_level = 2U;
      }
    } else if (std::strncmp(tok, "-gsplit-dwarf", 13U) == 0) {
      opts->split_dwarf_path = (tok[13] == '=') ? (tok + 14U) : "split.dwo";
      if (opts->debug_level == 0U) {
        opts->debug_level = 2U;
      }
    } else if (std::strcmp(tok, "-gdwarf-2") == 0 || std::strcmp(tok, "-gdwarf-3") == 0 ||
               std::strcmp(tok, "-gdwarf-4") == 0 || std::strcmp(tok, "-gdwarf-5") == 0) {
      opts->dwarf_version = static_cast<uint8_t>(tok[8] - '0');
      if (opts->debug_level == 0U) {
        opts->debug_level = 2U;
      }
    } else if (std::strcmp(tok, "-g") == 0 || std::strcmp(tok, "-g2") == 0 || std::strcmp(tok, "-ggdb") == 0) {
      opts->debug_level = 2U;
    } else if (std::strcmp(tok, "-g0") == 0) {
      opts->debug_level = 0U;
    } else if (std::strcmp(tok, "-g1") == 0) {
      opts->debug_level = 1U;
    } else if (std::strcmp(tok, "-g3") == 0 || std::strcmp(tok, "-ggdb3") == 0) {
      opts->debug_level = 3U;
    } else if (std::strcmp(tok, "-target") == 0) {
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 7U, &i, &opts->target_triple));
    } else if (std::strncmp(tok, "-march=", 7U) == 0) {
      opts->march = tok + 7U;
    } else if (std::strncmp(tok, "-mcpu=", 6U) == 0) {
      opts->mcpu = tok + 6U;
    } else if (std::strncmp(tok, "-mtune=", 7U) == 0) {
      opts->mtune = tok + 7U;
    } else if (std::strncmp(tok, "-mattr=", 7U) == 0) {
      opts->mattr = tok + 7U;
    } else if (std::strncmp(tok, "-mcmodel=", 9U) == 0) {
      opts->mcmodel = tok + 9U;
    } else if (std::strncmp(tok, "-mabi=", 6U) == 0) {
      opts->mabi = tok + 6U;
    } else if (std::strncmp(tok, "-mfpu=", 6U) == 0) {
      opts->mfpu = tok + 6U;
    } else if (std::strcmp(tok, "-m64") == 0 || std::strcmp(tok, "-m32") == 0 ||
               std::strncmp(tok, "-mmacosx-version-min=", 21U) == 0 ||
               std::strncmp(tok, "-miphoneos-version-min=", 23U) == 0 ||
               std::strncmp(tok, "-msse", 5U) == 0 || std::strncmp(tok, "-mavx", 5U) == 0 ||
               std::strcmp(tok, "-mfma") == 0 || std::strncmp(tok, "-mbmi", 5U) == 0 ||
               std::strcmp(tok, "-mpclmul") == 0 || std::strcmp(tok, "-maes") == 0 ||
               std::strcmp(tok, "-mcrc") == 0 || std::strcmp(tok, "-msha") == 0 ||
               std::strcmp(tok, "-mlzcnt") == 0 || std::strcmp(tok, "-mpopcnt") == 0) {
      EDG_BE_DRIVER_CHECK(push_list_item(out_cmd, out_cmd->extra_be_buf, &opts->num_extra_backend_args, tok));
    } else if (std::strcmp(tok, "-pthread") == 0) {
      opts->pthread_enabled = true;
      emit_prefix = "-D_REENTRANT=1";
    } else if (std::strcmp(tok, "-rdynamic") == 0) {
      opts->rdynamic_enabled = true;
    } else if (std::strcmp(tok, "-nostdlib") == 0) {
      opts->nostdlib_link = true;
    } else if (std::strcmp(tok, "-nodefaultlibs") == 0) {
      opts->nodefaultlibs_link = true;
    } else if (std::strcmp(tok, "-nostartfiles") == 0) {
      opts->nostartfiles_link = true;
    } else if (tok[1] == 'L') {
      const char* ldir = nullptr;
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 2U, &i, &ldir));
      EDG_BE_DRIVER_CHECK(push_list_item(out_cmd, out_cmd->lib_dirs_buf, &opts->num_library_search_dirs, ldir));
    } else if (tok[1] == 'l') {
      const char* lib = nullptr;
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 2U, &i, &lib));
      EDG_BE_DRIVER_CHECK(push_list_item(out_cmd, out_cmd->libs_buf, &opts->num_libraries, lib));
    } else if (std::strncmp(tok, "-Wl,", 4U) == 0) {
      EDG_BE_DRIVER_CHECK(parse_wl_passthrough(tok + 4U, out_cmd));
    } else if (std::strcmp(tok, "-Xlinker") == 0) {
      const char* xarg = nullptr;
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 8U, &i, &xarg));
      EDG_BE_DRIVER_CHECK(push_list_item(
          out_cmd, out_cmd->linker_args_buf, &opts->num_linker_passthrough_args, xarg));
    } else if (std::strcmp(tok, "-framework") == 0 ||
               std::strcmp(tok, "-weak_framework") == 0 ||
               std::strcmp(tok, "-install_name") == 0 ||
               std::strcmp(tok, "-compatibility_version") == 0 ||
               std::strcmp(tok, "-current_version") == 0 ||
               std::strcmp(tok, "-undefined") == 0 ||
               std::strcmp(tok, "-force_load") == 0 ||
               std::strcmp(tok, "-bundle_loader") == 0 ||
               std::strcmp(tok, "-exported_symbols_list") == 0 ||
               std::strcmp(tok, "-unexported_symbols_list") == 0) {
      const char* larg = nullptr;
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, std::strlen(tok), &i, &larg));
      EDG_BE_DRIVER_CHECK(push_list_item(
          out_cmd, out_cmd->linker_args_buf, &opts->num_linker_passthrough_args, tok));
      EDG_BE_DRIVER_CHECK(push_list_item(
          out_cmd, out_cmd->linker_args_buf, &opts->num_linker_passthrough_args, larg));
    } else if (std::strcmp(tok, "-headerpad_max_install_names") == 0 ||
               std::strcmp(tok, "-flat_namespace") == 0 ||
               std::strcmp(tok, "-dead_strip") == 0 ||
               std::strcmp(tok, "-search_paths_first") == 0 ||
               std::strcmp(tok, "-bundle") == 0 ||
               std::strcmp(tok, "-single_module") == 0 ||
               std::strncmp(tok, "-fuse-ld=", 9U) == 0) {
      EDG_BE_DRIVER_CHECK(push_list_item(
          out_cmd, out_cmd->linker_args_buf, &opts->num_linker_passthrough_args, tok));
    } else if (tok[1] == 'F') {
      const char* fdir = nullptr;
      EDG_BE_DRIVER_CHECK(consume_joined_or_next_arg(out_cmd, 2U, &i, &fdir));
      char fjoined[EDG_BE_SDK_MAX_PATH_LEN + 8U];
      std::snprintf(fjoined, sizeof(fjoined), "-F%s", fdir);
      EDG_BE_DRIVER_CHECK(push_list_item(
          out_cmd, out_cmd->linker_args_buf, &opts->num_linker_passthrough_args, fjoined));
    } else if (std::strcmp(tok, "-w") == 0) {
      emit_prefix = "-w";
    } else if (std::strcmp(tok, "-pedantic") == 0 || std::strcmp(tok, "-pedantic-errors") == 0 || tok[1] == 'W') {
      /* Accepted GCC/Clang warning flags. */
    } else if (tok[1] == 'f') {
      if (std::strcmp(tok, "-fPIC") == 0 || std::strcmp(tok, "-fpic") == 0) {
        opts->reloc_mode = edg_reloc_mode_t::pic;
      } else if (std::strcmp(tok, "-fPIE") == 0 || std::strcmp(tok, "-fpie") == 0) {
        opts->reloc_mode = edg_reloc_mode_t::pie;
      } else if (std::strcmp(tok, "-fno-pic") == 0 || std::strcmp(tok, "-fno-PIC") == 0 ||
                 std::strcmp(tok, "-fno-pie") == 0 || std::strcmp(tok, "-fno-PIE") == 0) {
        opts->reloc_mode = edg_reloc_mode_t::static_no_pic;
      } else if (std::strcmp(tok, "-fexceptions") == 0) {
        opts->exceptions_enabled = true;
      } else if (std::strcmp(tok, "-fno-exceptions") == 0) {
        opts->exceptions_enabled = false;
        emit_prefix = "--no_exceptions";
      } else if (std::strcmp(tok, "-frtti") == 0) {
        opts->rtti_enabled = true;
      } else if (std::strcmp(tok, "-fno-rtti") == 0) {
        opts->rtti_enabled = false;
        emit_prefix = "--no_rtti";
      } else if (std::strcmp(tok, "-funwind-tables") == 0 ||
                 std::strcmp(tok, "-fasynchronous-unwind-tables") == 0) {
        opts->unwind_tables = true;
      } else if (std::strcmp(tok, "-fno-unwind-tables") == 0) {
        opts->unwind_tables = false;
      } else if (std::strcmp(tok, "-fopenmp") == 0) {
        opts->openmp_enabled = true;
        emit_prefix = "--diag_suppress=161";
      } else if (std::strncmp(tok, "-fopenmp-targets=", 17U) == 0) {
        opts->openmp_enabled = true;
        opts->openmp_targets = tok + 17U;
        emit_prefix = "--diag_suppress=161";
      } else if (std::strcmp(tok, "-fmodules") == 0 || std::strcmp(tok, "-fmodules-ts") == 0) {
        saw_modules_flag = true;
        emit_prefix = "--modules";
      } else if (std::strncmp(tok, "-fmodule-output=", 16U) == 0) {
        saw_modules_flag = true;
        EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, "--modules", ""));
        emit_prefix = "--create_module_interface=";
        emit_val = tok + 16U;
      } else if (std::strncmp(tok, "-fprebuilt-module-path=", 23U) == 0) {
        saw_modules_flag = true;
        EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, "--modules", ""));
        emit_prefix = "--modules_directory=";
        emit_val = tok + 23U;
      } else if (std::strncmp(tok, "-fmodule-file=", 14U) == 0) {
        saw_modules_flag = true;
        EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, "--modules", ""));
        emit_prefix = "--ms_mod_file_map=";
        emit_val = tok + 14U;
      } else if (std::strcmp(tok, "-flto") == 0 || std::strcmp(tok, "-flto=full") == 0) {
        opts->lto_mode = edg_lto_mode_t::full_lto;
      } else if (std::strcmp(tok, "-flto=thin") == 0) {
        opts->lto_mode = edg_lto_mode_t::thin_lto;
      } else if (std::strcmp(tok, "-fno-lto") == 0) {
        opts->lto_mode = edg_lto_mode_t::none;
      } else if (std::strncmp(tok, "-fvisibility=", 13U) == 0) {
        const char* vis = tok + 13U;
        if (std::strcmp(vis, "default") == 0) {
          opts->symbol_visibility = edg_visibility_mode_t::default_vis;
        } else if (std::strcmp(vis, "hidden") == 0) {
          opts->symbol_visibility = edg_visibility_mode_t::hidden_vis;
        } else if (std::strcmp(vis, "protected") == 0) {
          opts->symbol_visibility = edg_visibility_mode_t::protected_vis;
        } else if (std::strcmp(vis, "internal") == 0) {
          opts->symbol_visibility = edg_visibility_mode_t::internal_vis;
        } else {
          return edg_be_driver_error_t::unrecognized_cli_flag;
        }
      } else if (std::strcmp(tok, "-fvisibility-inlines-hidden") == 0) {
        opts->visibility_inlines_hidden = true;
      } else if (std::strcmp(tok, "-ffunction-sections") == 0) {
        opts->function_sections = true;
      } else if (std::strcmp(tok, "-fdata-sections") == 0) {
        opts->data_sections = true;
      } else if (std::strcmp(tok, "-fomit-frame-pointer") == 0) {
        opts->omit_frame_pointer = true;
      } else if (std::strcmp(tok, "-fno-omit-frame-pointer") == 0) {
        opts->omit_frame_pointer = false;
      } else if (std::strcmp(tok, "-fsigned-char") == 0) {
        emit_prefix = "--signed_chars";
      } else if (std::strcmp(tok, "-funsigned-char") == 0) {
        emit_prefix = "--unsigned_chars";
      } else if (std::strcmp(tok, "-ffast-math") == 0) {
        opts->fast_math = true;
      } else if (std::strncmp(tok, "-fsanitize=", 11U) == 0) {
        EDG_BE_DRIVER_CHECK(apply_sanitize_list(tok + 11U, opts));
      }
    } else {
      return edg_be_driver_error_t::unrecognized_cli_flag;
    }

    if (emit_prefix != nullptr) {
      EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, emit_prefix, emit_val));
    }
  }

  if (saw_verbose_flag && opts->num_input_source_files == 0U &&
      opts->num_input_object_files == 0U && opts->query_kind == edg_driver_query_kind_t::none) {
    opts->query_kind = edg_driver_query_kind_t::print_verbose;
  }

  if (sdk_env != nullptr && !out_cmd->native_edg_mode) {
    edg_be_sdk_probe_env_t local_env = *sdk_env;
    if (opts->sysroot != nullptr) {
      local_env.explicit_sysroot = opts->sysroot;
    }
    local_env.nostdinc = opts->nostdinc;
    local_env.nostdincxx = opts->nostdincxx;
    local_env.nobuiltininc = opts->nobuiltininc;
    EDG_BE_DRIVER_CHECK(edg_be_sdk_discover(&local_env, &out_cmd->sdk_info));
    out_cmd->sdk_discovered = true;
  }

  if (opts->query_kind != edg_driver_query_kind_t::none) {
    return edg_be_driver_error_t::ok;
  }

  if (!out_cmd->native_edg_mode && opts->num_input_source_files > 0U) {
    EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, "--clear_flag=use_predefined_macro_file", ""));
    if (!saw_explicit_std_or_mode) {
      EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, opts->language_is_cplusplus ? "--c++17" : "--c17", ""));
    }
    if (opts->language_is_cplusplus) {
      EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, "--instantiate=used", ""));
    }
    if (out_cmd->sdk_discovered && !opts->nostdinc) {
      char ver_buf[32];
      std::snprintf(ver_buf, sizeof(ver_buf), "%u", out_cmd->sdk_info.compiler_version_code);
      const char* mode_flag = "--microsoft";
      const char* ver_prefix = "--microsoft_version=";
      if (out_cmd->sdk_info.flavor == edg_toolchain_flavor_t::clang_gnu) {
        mode_flag = "--clang";
        ver_prefix = "--clang_version=";
      } else if (out_cmd->sdk_info.flavor == edg_toolchain_flavor_t::gcc_gnu) {
        mode_flag = opts->language_is_cplusplus ? "--g++" : "--gcc";
        ver_prefix = "--gnu_version=";
      }
      if (!saw_explicit_emulation_flag) {
        EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, mode_flag, ""));
        EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, ver_prefix, ver_buf));
      }
      if (saw_modules_flag) {
        EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, "--modules", ""));
      }

      if (out_cmd->sdk_info.flavor != edg_toolchain_flavor_t::msvc_cl) {
        static const char* const kCommonGnuPredefs[] = {
          "-D__CHAR_BIT__=8",
          "-D__ORDER_LITTLE_ENDIAN__=1234",
          "-D__ORDER_BIG_ENDIAN__=4321",
          "-D__ORDER_PDP_ENDIAN__=3412",
          "-D__BYTE_ORDER__=1234",
          "-D__LITTLE_ENDIAN__=1",
          "-D_LP64=1",
          "-D__LP64__=1",
          "-D__GNUC__=4",
          "-D__GNUC_MINOR__=2",
          "-D__GNUC_PATCHLEVEL__=1",
          "-D__FINITE_MATH_ONLY__=0",
          "-D__ATOMIC_RELAXED=0",
          "-D__ATOMIC_CONSUME=1",
          "-D__ATOMIC_ACQUIRE=2",
          "-D__ATOMIC_RELEASE=3",
          "-D__ATOMIC_ACQ_REL=4",
          "-D__ATOMIC_SEQ_CST=5",
          "-D__GCC_ATOMIC_BOOL_LOCK_FREE=2",
          "-D__GCC_ATOMIC_CHAR_LOCK_FREE=2",
          "-D__GCC_ATOMIC_CHAR8_T_LOCK_FREE=2",
          "-D__GCC_ATOMIC_CHAR16_T_LOCK_FREE=2",
          "-D__GCC_ATOMIC_CHAR32_T_LOCK_FREE=2",
          "-D__GCC_ATOMIC_WCHAR_T_LOCK_FREE=2",
          "-D__GCC_ATOMIC_SHORT_LOCK_FREE=2",
          "-D__GCC_ATOMIC_INT_LOCK_FREE=2",
          "-D__GCC_ATOMIC_LONG_LOCK_FREE=2",
          "-D__GCC_ATOMIC_LLONG_LOCK_FREE=2",
          "-D__GCC_ATOMIC_POINTER_LOCK_FREE=2",
          "-D__GCC_ATOMIC_TEST_AND_SET_TRUEVAL=1",
          "-D__GCC_CONSTRUCTIVE_SIZE=64",
          "-D__GCC_DESTRUCTIVE_SIZE=256",
          "-D__GCC_HAVE_DWARF2_CFI_ASM=1",
          "-D__GCC_ASM_FLAG_OUTPUTS__=1",
          "-D__GXX_ABI_VERSION=1002",
          "-D__GXX_WEAK__=1",
          "-D__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1=1",
          "-D__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2=1",
          "-D__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4=1",
          "-D__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8=1",
          "-D__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16=1",
          "-D__SIZEOF_SHORT__=2",
          "-D__SIZEOF_INT__=4",
          "-D__SIZEOF_LONG__=8",
          "-D__SIZEOF_LONG_LONG__=8",
          "-D__SIZEOF_POINTER__=8",
          "-D__SIZEOF_FLOAT__=4",
          "-D__SIZEOF_DOUBLE__=8",
          "-D__SIZEOF_SIZE_T__=8",
          "-D__SIZEOF_PTRDIFF_T__=8",
          "-D__SIZEOF_WCHAR_T__=4",
          "-D__SIZEOF_WINT_T__=4",
          "-D__SIZEOF_INT128__=16",
          "-D__SIZE_TYPE__=long unsigned int",
          "-D__PTRDIFF_TYPE__=long int",
          "-D__INTPTR_TYPE__=long int",
          "-D__UINTPTR_TYPE__=long unsigned int",
          "-D__INTMAX_TYPE__=long int",
          "-D__UINTMAX_TYPE__=long unsigned int",
          "-D__WCHAR_TYPE__=int",
          "-D__WINT_TYPE__=int",
          "-D__INT8_TYPE__=signed char",
          "-D__INT16_TYPE__=short",
          "-D__INT32_TYPE__=int",
          "-D__INT64_TYPE__=long long int",
          "-D__UINT8_TYPE__=unsigned char",
          "-D__UINT16_TYPE__=unsigned short",
          "-D__UINT32_TYPE__=unsigned int",
          "-D__UINT64_TYPE__=long long unsigned int",
          "-D__INT8_MAX__=127",
          "-D__INT16_MAX__=32767",
          "-D__INT32_MAX__=2147483647",
          "-D__INT64_MAX__=9223372036854775807LL",
          "-D__UINT8_MAX__=255",
          "-D__UINT16_MAX__=65535",
          "-D__UINT32_MAX__=4294967295U",
          "-D__UINT64_MAX__=18446744073709551615ULL",
          "-D__SCHAR_MAX__=127",
          "-D__SHRT_MAX__=32767",
          "-D__INT_MAX__=2147483647",
          "-D__LONG_MAX__=9223372036854775807L",
          "-D__LONG_LONG_MAX__=9223372036854775807LL",
          "-D__SIZE_MAX__=18446744073709551615UL",
          "-D__PTRDIFF_MAX__=9223372036854775807L",
          "-D__INTPTR_MAX__=9223372036854775807L",
          "-D__UINTPTR_MAX__=18446744073709551615UL",
          "-D__INTMAX_MAX__=9223372036854775807L",
          "-D__UINTMAX_MAX__=18446744073709551615UL",
          "-D__WCHAR_MAX__=2147483647",
          "-D__WCHAR_WIDTH__=32",
          "-D__WINT_MAX__=2147483647",
          "-D__WINT_WIDTH__=32",
          "-D__INT_WIDTH__=32",
          "-D__SIZE_WIDTH__=64",
          "-D__PTRDIFF_WIDTH__=64",
          "-D__INTPTR_WIDTH__=64",
          "-D__UINTPTR_WIDTH__=64",
          "-D__INTMAX_WIDTH__=64",
          "-D__UINTMAX_WIDTH__=64",
          "-D__INT_LEAST8_TYPE__=signed char",
          "-D__INT_LEAST16_TYPE__=short",
          "-D__INT_LEAST32_TYPE__=int",
          "-D__INT_LEAST64_TYPE__=long long int",
          "-D__UINT_LEAST8_TYPE__=unsigned char",
          "-D__UINT_LEAST16_TYPE__=unsigned short",
          "-D__UINT_LEAST32_TYPE__=unsigned int",
          "-D__UINT_LEAST64_TYPE__=long long unsigned int",
          "-D__INT_FAST8_TYPE__=signed char",
          "-D__INT_FAST16_TYPE__=short",
          "-D__INT_FAST32_TYPE__=int",
          "-D__INT_FAST64_TYPE__=long long int",
          "-D__UINT_FAST8_TYPE__=unsigned char",
          "-D__UINT_FAST16_TYPE__=unsigned short",
          "-D__UINT_FAST32_TYPE__=unsigned int",
          "-D__UINT_FAST64_TYPE__=long long unsigned int",
          "-D__INT_LEAST8_MAX__=127",
          "-D__INT_LEAST16_MAX__=32767",
          "-D__INT_LEAST32_MAX__=2147483647",
          "-D__INT_LEAST64_MAX__=9223372036854775807LL",
          "-D__UINT_LEAST8_MAX__=255",
          "-D__UINT_LEAST16_MAX__=65535",
          "-D__UINT_LEAST32_MAX__=4294967295U",
          "-D__UINT_LEAST64_MAX__=18446744073709551615ULL",
          "-D__INT_FAST8_MAX__=127",
          "-D__INT_FAST16_MAX__=32767",
          "-D__INT_FAST32_MAX__=2147483647",
          "-D__INT_FAST64_MAX__=9223372036854775807LL",
          "-D__UINT_FAST8_MAX__=255",
          "-D__UINT_FAST16_MAX__=65535",
          "-D__UINT_FAST32_MAX__=4294967295U",
          "-D__UINT_FAST64_MAX__=18446744073709551615ULL",
          "-D__INT_LEAST8_WIDTH__=8",
          "-D__INT_LEAST16_WIDTH__=16",
          "-D__INT_LEAST32_WIDTH__=32",
          "-D__INT_LEAST64_WIDTH__=64",
          "-D__INT_FAST8_WIDTH__=8",
          "-D__INT_FAST16_WIDTH__=16",
          "-D__INT_FAST32_WIDTH__=32",
          "-D__INT_FAST64_WIDTH__=64",
          "-D__INT8_C_SUFFIX__=",
          "-D__INT16_C_SUFFIX__=",
          "-D__INT32_C_SUFFIX__=",
          "-D__INT64_C_SUFFIX__=LL",
          "-D__UINT8_C_SUFFIX__=",
          "-D__UINT16_C_SUFFIX__=",
          "-D__UINT32_C_SUFFIX__=U",
          "-D__UINT64_C_SUFFIX__=ULL",
          "-D__INTMAX_C_SUFFIX__=L",
          "-D__UINTMAX_C_SUFFIX__=UL",
          "-D__INT8_FMTd__=\"hhd\"",
          "-D__INT8_FMTi__=\"hhi\"",
          "-D__UINT8_FMTo__=\"hho\"",
          "-D__UINT8_FMTu__=\"hhu\"",
          "-D__UINT8_FMTx__=\"hhx\"",
          "-D__UINT8_FMTX__=\"hhX\"",
          "-D__INT16_FMTd__=\"hd\"",
          "-D__INT16_FMTi__=\"hi\"",
          "-D__UINT16_FMTo__=\"ho\"",
          "-D__UINT16_FMTu__=\"hu\"",
          "-D__UINT16_FMTx__=\"hx\"",
          "-D__UINT16_FMTX__=\"hX\"",
          "-D__INT32_FMTd__=\"d\"",
          "-D__INT32_FMTi__=\"i\"",
          "-D__UINT32_FMTo__=\"o\"",
          "-D__UINT32_FMTu__=\"u\"",
          "-D__UINT32_FMTx__=\"x\"",
          "-D__UINT32_FMTX__=\"X\"",
          "-D__INT64_FMTd__=\"lld\"",
          "-D__INT64_FMTi__=\"lli\"",
          "-D__UINT64_FMTo__=\"llo\"",
          "-D__UINT64_FMTu__=\"llu\"",
          "-D__UINT64_FMTx__=\"llx\"",
          "-D__UINT64_FMTX__=\"llX\"",
          "-D__INT_LEAST8_FMTd__=\"hhd\"",
          "-D__INT_LEAST8_FMTi__=\"hhi\"",
          "-D__UINT_LEAST8_FMTo__=\"hho\"",
          "-D__UINT_LEAST8_FMTu__=\"hhu\"",
          "-D__UINT_LEAST8_FMTx__=\"hhx\"",
          "-D__UINT_LEAST8_FMTX__=\"hhX\"",
          "-D__INT_LEAST16_FMTd__=\"hd\"",
          "-D__INT_LEAST16_FMTi__=\"hi\"",
          "-D__UINT_LEAST16_FMTo__=\"ho\"",
          "-D__UINT_LEAST16_FMTu__=\"hu\"",
          "-D__UINT_LEAST16_FMTx__=\"hx\"",
          "-D__UINT_LEAST16_FMTX__=\"hX\"",
          "-D__INT_LEAST32_FMTd__=\"d\"",
          "-D__INT_LEAST32_FMTi__=\"i\"",
          "-D__UINT_LEAST32_FMTo__=\"o\"",
          "-D__UINT_LEAST32_FMTu__=\"u\"",
          "-D__UINT_LEAST32_FMTx__=\"x\"",
          "-D__UINT_LEAST32_FMTX__=\"X\"",
          "-D__INT_LEAST64_FMTd__=\"lld\"",
          "-D__INT_LEAST64_FMTi__=\"lli\"",
          "-D__UINT_LEAST64_FMTo__=\"llo\"",
          "-D__UINT_LEAST64_FMTu__=\"llu\"",
          "-D__UINT_LEAST64_FMTx__=\"llx\"",
          "-D__UINT_LEAST64_FMTX__=\"llX\"",
          "-D__INT_FAST8_FMTd__=\"hhd\"",
          "-D__INT_FAST8_FMTi__=\"hhi\"",
          "-D__UINT_FAST8_FMTo__=\"hho\"",
          "-D__UINT_FAST8_FMTu__=\"hhu\"",
          "-D__UINT_FAST8_FMTx__=\"hhx\"",
          "-D__UINT_FAST8_FMTX__=\"hhX\"",
          "-D__INT_FAST16_FMTd__=\"hd\"",
          "-D__INT_FAST16_FMTi__=\"hi\"",
          "-D__UINT_FAST16_FMTo__=\"ho\"",
          "-D__UINT_FAST16_FMTu__=\"hu\"",
          "-D__UINT_FAST16_FMTx__=\"hx\"",
          "-D__UINT_FAST16_FMTX__=\"hX\"",
          "-D__INT_FAST32_FMTd__=\"d\"",
          "-D__INT_FAST32_FMTi__=\"i\"",
          "-D__UINT_FAST32_FMTo__=\"o\"",
          "-D__UINT_FAST32_FMTu__=\"u\"",
          "-D__UINT_FAST32_FMTx__=\"x\"",
          "-D__UINT_FAST32_FMTX__=\"X\"",
          "-D__INT_FAST64_FMTd__=\"lld\"",
          "-D__INT_FAST64_FMTi__=\"lli\"",
          "-D__UINT_FAST64_FMTo__=\"llo\"",
          "-D__UINT_FAST64_FMTu__=\"llu\"",
          "-D__UINT_FAST64_FMTx__=\"llx\"",
          "-D__UINT_FAST64_FMTX__=\"llX\"",
          "-D__INTPTR_FMTd__=\"ld\"",
          "-D__INTPTR_FMTi__=\"li\"",
          "-D__UINTPTR_FMTo__=\"lo\"",
          "-D__UINTPTR_FMTu__=\"lu\"",
          "-D__UINTPTR_FMTx__=\"lx\"",
          "-D__UINTPTR_FMTX__=\"lX\"",
          "-D__PTRDIFF_FMTd__=\"ld\"",
          "-D__PTRDIFF_FMTi__=\"li\"",
          "-D__SIZE_FMTo__=\"lo\"",
          "-D__SIZE_FMTu__=\"lu\"",
          "-D__SIZE_FMTx__=\"lx\"",
          "-D__SIZE_FMTX__=\"lX\"",
          "-D__INTMAX_FMTd__=\"ld\"",
          "-D__INTMAX_FMTi__=\"li\"",
          "-D__UINTMAX_FMTo__=\"lo\"",
          "-D__UINTMAX_FMTu__=\"lu\"",
          "-D__UINTMAX_FMTx__=\"lx\"",
          "-D__UINTMAX_FMTX__=\"lX\"",
          "-D__INT8_C(c)=c",
          "-D__INT16_C(c)=c",
          "-D__INT32_C(c)=c",
          "-D__INT64_C(c)=c##LL",
          "-D__UINT8_C(c)=c",
          "-D__UINT16_C(c)=c",
          "-D__UINT32_C(c)=c##U",
          "-D__UINT64_C(c)=c##ULL",
          "-D__INTMAX_C(c)=c##L",
          "-D__UINTMAX_C(c)=c##UL",
          "-D__FLT_RADIX__=2",
          "-D__FLT_MANT_DIG__=24",
          "-D__FLT_DIG__=6",
          "-D__FLT_DECIMAL_DIG__=9",
          "-D__FLT_MIN_EXP__=(-125)",
          "-D__FLT_MIN_10_EXP__=(-37)",
          "-D__FLT_MAX_EXP__=128",
          "-D__FLT_MAX_10_EXP__=38",
          "-D__FLT_MAX__=3.40282347e+38F",
          "-D__FLT_MIN__=1.17549435e-38F",
          "-D__FLT_EPSILON__=1.19209290e-7F",
          "-D__FLT_DENORM_MIN__=1.40129846e-45F",
          "-D__FLT_HAS_DENORM__=1",
          "-D__FLT_HAS_INFINITY__=1",
          "-D__FLT_HAS_QUIET_NAN__=1",
          "-D__DBL_MANT_DIG__=53",
          "-D__DBL_DIG__=15",
          "-D__DBL_DECIMAL_DIG__=17",
          "-D__DBL_MIN_EXP__=(-1021)",
          "-D__DBL_MIN_10_EXP__=(-307)",
          "-D__DBL_MAX_EXP__=1024",
          "-D__DBL_MAX_10_EXP__=308",
          "-D__DBL_MAX__=1.7976931348623157e+308",
          "-D__DBL_MIN__=2.2250738585072014e-308",
          "-D__DBL_EPSILON__=2.2204460492503131e-16",
          "-D__DBL_DENORM_MIN__=4.9406564584124654e-324",
          "-D__DBL_HAS_DENORM__=1",
          "-D__DBL_HAS_INFINITY__=1",
          "-D__DBL_HAS_QUIET_NAN__=1"
        };
        for (size_t m = 0; m < sizeof(kCommonGnuPredefs) / sizeof(kCommonGnuPredefs[0]); ++m) {
          EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, kCommonGnuPredefs[m], ""));
        }
        const char* trip = out_cmd->sdk_info.detected_triple;
        const bool is_darwin = (std::strstr(trip, "darwin") != nullptr);
        const bool is_arm64 = (std::strstr(trip, "arm64") != nullptr ||
                               std::strstr(trip, "aarch64") != nullptr);
        if (is_darwin) {
          EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, "-D__APPLE__=1", ""));
          EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, "-D__MACH__=1", ""));
          EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, "-D__APPLE_CC__=6000", ""));
          EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, "-D__apple_build_version__=17000000", ""));
          EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, "-D__USER_LABEL_PREFIX__=_", ""));
        } else {
          EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, "-D__linux__=1", ""));
          EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, "-D__unix__=1", ""));
          EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, "-D__USER_LABEL_PREFIX__=", ""));
        }
        if (is_arm64) {
          static const char* const kArm64Predefs[] = {
            "-D__arm64=1", "-D__arm64__=1", "-D__aarch64__=1", "-D__AARCH64EL__=1",
            "-D__ARM_ARCH=8", "-D__BIGGEST_ALIGNMENT__=16",
            "-D__SIZEOF_LONG_DOUBLE__=8", "-D__LDBL_MANT_DIG__=53", "-D__LDBL_DIG__=15",
            "-D__LDBL_DECIMAL_DIG__=17", "-D__DECIMAL_DIG__=17",
            "-D__LDBL_MIN_EXP__=(-1021)", "-D__LDBL_MIN_10_EXP__=(-307)",
            "-D__LDBL_MAX_EXP__=1024", "-D__LDBL_MAX_10_EXP__=308",
            "-D__LDBL_MAX__=1.7976931348623157e+308L",
            "-D__LDBL_MIN__=2.2250738585072014e-308L",
            "-D__LDBL_EPSILON__=2.2204460492503131e-16L",
            "-D__LDBL_DENORM_MIN__=4.9406564584124654e-324L",
            "-D__LDBL_HAS_DENORM__=1", "-D__LDBL_HAS_INFINITY__=1", "-D__LDBL_HAS_QUIET_NAN__=1"
          };
          for (size_t m = 0; m < sizeof(kArm64Predefs) / sizeof(kArm64Predefs[0]); ++m) {
            EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, kArm64Predefs[m], ""));
          }
        } else {
          static const char* const kX86Predefs[] = {
            "-D__x86_64=1", "-D__x86_64__=1", "-D__amd64=1", "-D__amd64__=1",
            "-D__SSE__=1", "-D__SSE2__=1", "-D__MMX__=1", "-D__BIGGEST_ALIGNMENT__=16",
            "-D__SIZEOF_LONG_DOUBLE__=16", "-D__LDBL_MANT_DIG__=64", "-D__LDBL_DIG__=18",
            "-D__LDBL_DECIMAL_DIG__=21", "-D__DECIMAL_DIG__=21",
            "-D__LDBL_MIN_EXP__=(-16381)", "-D__LDBL_MIN_10_EXP__=(-4931)",
            "-D__LDBL_MAX_EXP__=16384", "-D__LDBL_MAX_10_EXP__=4932",
            "-D__LDBL_MAX__=1.18973149535723176502e+4932L",
            "-D__LDBL_MIN__=3.36210314311209350626e-4932L",
            "-D__LDBL_EPSILON__=1.08420217248550443401e-19L",
            "-D__LDBL_DENORM_MIN__=3.64519953188247460253e-4951L",
            "-D__LDBL_HAS_DENORM__=1", "-D__LDBL_HAS_INFINITY__=1", "-D__LDBL_HAS_QUIET_NAN__=1"
          };
          for (size_t m = 0; m < sizeof(kX86Predefs) / sizeof(kX86Predefs[0]); ++m) {
            EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, kX86Predefs[m], ""));
          }
        }
      }

      if (opts->language_is_cplusplus && !opts->nostdincxx) {
        for (size_t k = 0; k < out_cmd->sdk_info.num_cxx_include_dirs; ++k) {
          EDG_BE_DRIVER_CHECK(push_edg_joined_arg(
              out_cmd, "--sys_include=", out_cmd->sdk_info.cxx_include_dirs[k]));
        }
      }
      for (size_t k = 0; k < out_cmd->sdk_info.num_c_include_dirs; ++k) {
        EDG_BE_DRIVER_CHECK(push_edg_joined_arg(
            out_cmd, "--sys_include=", out_cmd->sdk_info.c_include_dirs[k]));
      }
    }
  }

  for (size_t s = 0; s < opts->num_input_source_files; ++s) {
    EDG_BE_DRIVER_CHECK(push_edg_joined_arg(out_cmd, opts->input_source_files[s], ""));
  }

  return edg_be_driver_error_t::ok;
}

/**
 * @brief Executes an immediate build-system introspection query (`--version`, `-dumpmachine`, etc.).
 * @details Formats Clang/GCC-compatible lines into `out_stream` and checks stream status.
 * @param[in] opts Normalized invocation options with `query_kind != edg_driver_query_kind_t::none`.
 * @param[in] sdk_info Optional discovered SDK metadata (may be nullptr).
 * @param[in,out] out_stream Writable `FILE*` stream.
 * @return `edg_be_driver_error_t::ok` on success, `invalid_argument` on null/none query, or `io_error`.
 */
edg_be_driver_error_t edg_be_execute_driver_query(
    const edg_be_invocation_options_t* opts,
    const edg_be_sdk_result_t* sdk_info,
    FILE* out_stream) noexcept
{
  EDG_BE_DRIVER_CHECK_NOT_NULL(opts);
  EDG_BE_DRIVER_CHECK_NOT_NULL(out_stream);

  const char* triple = "x86_64-unknown-linux-gnu";
  if (opts->target_triple != nullptr && opts->target_triple[0] != '\0') {
    triple = opts->target_triple;
  } else if (sdk_info != nullptr && sdk_info->detected_triple[0] != '\0') {
    triple = sdk_info->detected_triple;
  }

  const char* res_dir = (sdk_info != nullptr && sdk_info->detected_resource_dir[0] != '\0')
                            ? sdk_info->detected_resource_dir
                            : "/usr/include";
  const char* sysroot = (sdk_info != nullptr && sdk_info->detected_sysroot[0] != '\0')
                            ? sdk_info->detected_sysroot
                            : "/";

  char qbuf[2048];
  switch (opts->query_kind) {
    case edg_driver_query_kind_t::none:
      return edg_be_driver_error_t::invalid_argument;
    case edg_driver_query_kind_t::print_version:
#ifdef BUILD_IDENTIFIER
      std::snprintf(qbuf, sizeof(qbuf),
                    "Edison Design Group C/C++ Front End, version 7.0 (" BUILD_IDENTIFIER ")%cTarget: %s%c",
                    kCharLf, triple, kCharLf);
#else
      std::snprintf(qbuf, sizeof(qbuf),
                    "Edison Design Group C/C++ Front End, version 7.0 (" __DATE__ " " __TIME__ ")%cTarget: %s%c",
                    kCharLf, triple, kCharLf);
#endif
      break;
    case edg_driver_query_kind_t::print_verbose:
      std::snprintf(qbuf, sizeof(qbuf), "EDG multi-backend compiler version 7.0.1%cTarget: %s%cSysroot: %s%c",
                    kCharLf, triple, kCharLf, sysroot, kCharLf);
      break;
    case edg_driver_query_kind_t::dump_machine:
      std::snprintf(qbuf, sizeof(qbuf), "%s%c", triple, kCharLf);
      break;
    case edg_driver_query_kind_t::dump_version:
      std::snprintf(qbuf, sizeof(qbuf), "17%c", kCharLf);
      break;
    case edg_driver_query_kind_t::dump_full_version:
      std::snprintf(qbuf, sizeof(qbuf), "17.0.0%c", kCharLf);
      break;
    case edg_driver_query_kind_t::print_search_dirs:
      std::snprintf(qbuf, sizeof(qbuf), "install: %s%cprograms: =/usr/bin%clibraries: =%s/usr/lib%c",
                    res_dir, kCharLf, kCharLf, sysroot, kCharLf);
      break;
    case edg_driver_query_kind_t::print_prog_name:
    case edg_driver_query_kind_t::print_file_name:
      std::snprintf(qbuf, sizeof(qbuf), "%s%c",
                    (opts->query_argument != nullptr) ? opts->query_argument : "", kCharLf);
      break;
    case edg_driver_query_kind_t::print_libgcc_file_name:
      std::snprintf(qbuf, sizeof(qbuf), "libgcc.a%c", kCharLf);
      break;
    case edg_driver_query_kind_t::print_resource_dir:
      std::snprintf(qbuf, sizeof(qbuf), "%s%c", res_dir, kCharLf);
      break;
    default:
      return edg_be_driver_error_t::invalid_argument;
  }
  const size_t qlen = std::strlen(qbuf);
  const size_t nwritten = std::fwrite(qbuf, 1U, qlen, out_stream);
  std::fflush(out_stream);
  if (nwritten != qlen) {
    return edg_be_driver_error_t::io_error;
  }
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Derives the POSIX-standard current-working-directory object filename (`<stem>.o`).
 * @details Strips leading `/` and backslash directory prefixes and replaces the final extension.
 * @param[in] source_path Input source path (e.g., `"/ro/src/foo.cpp"`).
 * @param[in] use_obj_ext When true, appends `".obj"`; otherwise appends `".o"`.
 * @param[out] out_buf Destination buffer for derived object filename.
 * @param[in] buf_cap Byte capacity of `out_buf`.
 * @return `edg_be_driver_error_t::ok` on success, `invalid_argument` on null/empty input, or `out_of_memory`.
 */
edg_be_driver_error_t edg_be_derive_cwd_object_name(
    const char* source_path,
    bool use_obj_ext,
    char* out_buf,
    size_t buf_cap) noexcept
{
  EDG_BE_DRIVER_CHECK_NOT_NULL(source_path);
  EDG_BE_DRIVER_CHECK_NOT_NULL(out_buf);
  if (source_path[0] == '\0') {
    return edg_be_driver_error_t::invalid_argument;
  }

  const char* base = source_path;
  for (const char* p = source_path; *p != '\0'; ++p) {
    if (*p == '/' || *p == kCharBackslash) {
      base = p + 1;
    }
  }
  if (base[0] == '\0') {
    return edg_be_driver_error_t::invalid_argument;
  }

  const char* dot = std::strrchr(base, '.');
  const size_t stem_len = (dot != nullptr && dot != base)
                              ? static_cast<size_t>(dot - base)
                              : std::strlen(base);
  const char* ext = use_obj_ext ? ".obj" : ".o";
  const size_t ext_len = std::strlen(ext);
  if (stem_len + ext_len + 1U > buf_cap) {
    return edg_be_driver_error_t::out_of_memory;
  }
  std::memcpy(out_buf, base, stem_len);
  std::memcpy(out_buf + stem_len, ext, ext_len + 1U);
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Formats Make dependency rules into a caller-supplied character buffer.
 * @details Escapes spaces (`\ `), `#` (`\#`), and `$` (`$$`), honors `-MQ` pre-quoted targets,
 * and appends `-MP` phony header rules when enabled.
 * @param[in] cmd Translated command holding `options` and `dep_target_is_quoted` metadata.
 * @param[in] primary_source Path to the primary source file being compiled.
 * @param[in] included_files Array of header file paths included during preprocessing.
 * @param[in] num_included_files Number of entries in `included_files`.
 * @param[out] out_buf Destination character buffer.
 * @param[in] buf_cap Byte capacity of `out_buf`.
 * @param[out] out_len Receives the number of formatted bytes written (excluding null terminator).
 * @return `edg_be_driver_error_t::ok` on success, `invalid_argument` on null args, or `out_of_memory`.
 */
edg_be_driver_error_t edg_be_format_depfile_buffer(
    const edg_be_translated_command_t* cmd,
    const char* primary_source,
    const char* const* included_files,
    size_t num_included_files,
    char* out_buf,
    size_t buf_cap,
    size_t* out_len) noexcept
{
  EDG_BE_DRIVER_CHECK_NOT_NULL(cmd);
  EDG_BE_DRIVER_CHECK_NOT_NULL(primary_source);
  EDG_BE_DRIVER_CHECK_NOT_NULL(out_buf);
  EDG_BE_DRIVER_CHECK_NOT_NULL(out_len);
  if (buf_cap == 0U) {
    return edg_be_driver_error_t::out_of_memory;
  }
  if (num_included_files > 0U) {
    EDG_BE_DRIVER_CHECK_NOT_NULL(included_files);
  }

  out_buf[0] = '\0';
  size_t pos = 0U;
  const edg_be_invocation_options_t* opts = &cmd->options;

  char default_obj[256];
  EDG_BE_DRIVER_CHECK(edg_be_derive_cwd_object_name(primary_source, false, default_obj, sizeof(default_obj)));

  if (opts->num_dep_targets > 0U) {
    for (size_t t = 0; t < opts->num_dep_targets; ++t) {
      if (t > 0U) {
        EDG_BE_DRIVER_CHECK(depbuf_append_raw(out_buf, buf_cap, &pos, " ", 1U));
      }
      EDG_BE_DRIVER_CHECK(depbuf_append_escaped(
          out_buf, buf_cap, &pos, opts->dep_targets[t], cmd->dep_target_is_quoted[t]));
    }
  } else {
    const char* default_target = (opts->output_file_path != nullptr && opts->output_file_path[0] != '\0')
                                     ? opts->output_file_path
                                     : default_obj;
    EDG_BE_DRIVER_CHECK(depbuf_append_escaped(out_buf, buf_cap, &pos, default_target, false));
  }

  EDG_BE_DRIVER_CHECK(depbuf_append_raw(out_buf, buf_cap, &pos, ": ", 2U));
  EDG_BE_DRIVER_CHECK(depbuf_append_escaped(out_buf, buf_cap, &pos, primary_source, false));

  for (size_t h = 0; h < num_included_files; ++h) {
    if (included_files[h] != nullptr && included_files[h][0] != '\0') {
      EDG_BE_DRIVER_CHECK(depbuf_append_raw(out_buf, buf_cap, &pos, " ", 1U));
      EDG_BE_DRIVER_CHECK(depbuf_append_escaped(out_buf, buf_cap, &pos, included_files[h], false));
    }
  }
  EDG_BE_DRIVER_CHECK(depbuf_append_raw(out_buf, buf_cap, &pos, &kCharLf, 1U));

  if (opts->dep_phony_targets) {
    for (size_t h = 0; h < num_included_files; ++h) {
      if (included_files[h] != nullptr && included_files[h][0] != '\0') {
        EDG_BE_DRIVER_CHECK(depbuf_append_raw(out_buf, buf_cap, &pos, &kCharLf, 1U));
        EDG_BE_DRIVER_CHECK(depbuf_append_escaped(out_buf, buf_cap, &pos, included_files[h], false));
        const char colon_lf[2] = {':', kCharLf};
        EDG_BE_DRIVER_CHECK(depbuf_append_raw(out_buf, buf_cap, &pos, colon_lf, 2U));
      }
    }
  }

  *out_len = pos;
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Formats and writes Make dependency rules into an already-open `FILE*` stream.
 * @details Formats via `edg_be_format_depfile_buffer` and verifies `std::fwrite` completion.
 * @param[in] cmd Translated command holding `options` and `dep_target_is_quoted` metadata.
 * @param[in] primary_source Path to the primary source file being compiled.
 * @param[in] included_files Array of header file paths included during preprocessing.
 * @param[in] num_included_files Number of entries in `included_files`.
 * @param[in,out] out_stream Open writable `FILE*` stream.
 * @return `edg_be_driver_error_t::ok` on success, `invalid_argument`, `out_of_memory`, or `depfile_write_failed`.
 */
edg_be_driver_error_t edg_be_write_depfile_stream(
    const edg_be_translated_command_t* cmd,
    const char* primary_source,
    const char* const* included_files,
    size_t num_included_files,
    FILE* out_stream) noexcept
{
  EDG_BE_DRIVER_CHECK_NOT_NULL(out_stream);
  static char s_dep_buf[2097152];
  size_t dep_len = 0U;
  EDG_BE_DRIVER_CHECK(edg_be_format_depfile_buffer(
      cmd, primary_source, included_files, num_included_files, s_dep_buf, sizeof(s_dep_buf), &dep_len));
  const size_t written = std::fwrite(s_dep_buf, 1U, dep_len, out_stream);
  std::fflush(out_stream);
  if (written != dep_len) {
    return edg_be_driver_error_t::depfile_write_failed;
  }
  return edg_be_driver_error_t::ok;
}

/**
 * @brief Formats and writes a GNU Make / Ninja dependency file (`-MD`, `-MMD`, `-MF`, `-MT`, `-MQ`, `-MP`).
 * @details Derives `<output>.d` or `<stem>.d` when `-MF` was not explicitly passed.
 * @param[in] cmd Translated command holding `options` and `dep_target_is_quoted` metadata.
 * @param[in] primary_source Path to the primary source file being compiled.
 * @param[in] included_files Array of header file paths included during preprocessing.
 * @param[in] num_included_files Number of entries in `included_files`.
 * @return `edg_be_driver_error_t::ok` on success, `invalid_argument` on null args, or `depfile_write_failed`.
 */
edg_be_driver_error_t edg_be_write_depfile(
    const edg_be_translated_command_t* cmd,
    const char* primary_source,
    const char* const* included_files,
    size_t num_included_files) noexcept
{
  EDG_BE_DRIVER_CHECK_NOT_NULL(cmd);
  EDG_BE_DRIVER_CHECK_NOT_NULL(primary_source);
  if (num_included_files > 0U) {
    EDG_BE_DRIVER_CHECK_NOT_NULL(included_files);
  }
  const edg_be_invocation_options_t* opts = &cmd->options;
  if (!opts->generate_depfile) {
    return edg_be_driver_error_t::ok;
  }

  char default_obj[256];
  EDG_BE_DRIVER_CHECK(edg_be_derive_cwd_object_name(primary_source, false, default_obj, sizeof(default_obj)));

  char derived_mf[EDG_BE_SDK_MAX_PATH_LEN];
  const char* mf_path = opts->dep_file_path;
  if (mf_path == nullptr || mf_path[0] == '\0') {
    const char* base_out = (opts->output_file_path != nullptr && opts->output_file_path[0] != '\0')
                               ? opts->output_file_path
                               : default_obj;
    const char* dot = std::strrchr(base_out, '.');
    const size_t stem_len = (dot != nullptr) ? static_cast<size_t>(dot - base_out) : std::strlen(base_out);
    if (stem_len + 3U > sizeof(derived_mf)) {
      return edg_be_driver_error_t::out_of_memory;
    }
    std::memcpy(derived_mf, base_out, stem_len);
    std::memcpy(derived_mf + stem_len, ".d", 3U);
    mf_path = derived_mf;
  }

  FILE* fp = std::fopen(mf_path, "wb");
  if (fp == nullptr) {
    return edg_be_driver_error_t::depfile_write_failed;
  }
  const edg_be_driver_error_t write_err =
      edg_be_write_depfile_stream(cmd, primary_source, included_files, num_included_files, fp);
  std::fclose(fp);
  return write_err;
}

END_EDG_NAMESPACE
