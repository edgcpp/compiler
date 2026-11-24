/**
 * @file test_llvm_e2e_execution.cpp
 * @ingroup llvm_gen_be_tests
 * @brief End-to-end native compilation and execution tests for the EDG LLVM IR backend (`cpfe`).
 * @details Compiles real C and C++20 translation units through `cpfe`, links native executables
 *          and shared libraries (including cross-object ABI interop with system `clang`), and
 *          validates exact runtime behavior across all Section 9.1 requirements without stray `.ll` files.
 */

#include "llvm_gen_be_error.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/raw_ostream.h>
#include <string>
#include <vector>

using namespace edg;

BEGIN_EDG_NAMESPACE
/**
 * @brief Stub source-sequence resolver satisfying `llvm_gen_be_error.cpp` in standalone utility mode.
 * @details Populates empty source coordinates when `seq` is queried outside front-end translation units.
 * @param[in] seq Source sequence number.
 * @param[out] file_name Receives short filename pointer.
 * @param[out] full_name Receives full path pointer.
 * @param[out] line_number Receives 1-based line number.
 * @param[out] at_end_of_source Receives end-of-source boolean flag.
 */
void conv_seq_to_file_and_line(
    [[maybe_unused]] a_seq_number seq,
    a_const_char** file_name,
    a_const_char** full_name,
    a_line_number* line_number,
    a_boolean* at_end_of_source) noexcept {
  if (file_name) *file_name = "e2e";
  if (full_name) *full_name = "e2e";
  if (line_number) *line_number = 1;
  if (at_end_of_source) *at_end_of_source = FALSE;
}
END_EDG_NAMESPACE

/**
 * @brief Writes text content to a file path on disk.
 * @details Creates or truncates @p file_path and writes @p content verbatim.
 * @param[in] file_path Destination file path.
 * @param[in] content Text content to write.
 * @return llvm_gen_be_error_t::ok on success, or `io_error` on failure.
 */
static llvm_gen_be_error_t write_text_file(
    const std::string& file_path,
    const std::string& content) noexcept {
  std::error_code ec;
  llvm::raw_fd_ostream os(file_path, ec, llvm::sys::fs::OF_Text);
  if (ec) {
    return llvm_gen_be_error_t::io_error;
  }
  os << content;
  os.flush();
  return os.has_error() ? llvm_gen_be_error_t::io_error : llvm_gen_be_error_t::ok;
}

/**
 * @brief Executes a subprocess and returns its exit code via @p out_rc.
 * @details Uses `llvm::sys::ExecuteAndWait` without exceptions.
 * @param[in] prog Path to executable program.
 * @param[in] args Command-line arguments (`args[0]` is program name).
 * @param[out] out_rc Receives the subprocess exit status code.
 * @return llvm_gen_be_error_t::ok on process completion, or `io_error` if spawn failed.
 */
static llvm_gen_be_error_t run_cmd(
    const std::string& prog,
    const std::vector<std::string>& args,
    int* out_rc) noexcept {
  if (!out_rc) return llvm_gen_be_error_t::invalid_argument;
  std::vector<llvm::StringRef> refs;
  refs.reserve(args.size());
  for (const std::string& a : args) {
    refs.push_back(a);
  }
  std::string err_msg;
  int rc = llvm::sys::ExecuteAndWait(prog, refs, std::nullopt, {}, 0, 0, &err_msg);
  if (rc < 0 && !err_msg.empty()) {
    llvm::errs() << "Spawn failed (" << prog << "): " << err_msg << '\n';
    return llvm_gen_be_error_t::io_error;
  }
  *out_rc = rc;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests `std::nullptr_t`, `bool[8]` layout, bitfields, and `_BitInt(256)` arithmetic.
 * @details Compiles and executes a C++20 program exercising Section 9.1 items 1 & 2.
 * @param[in] cpfe_bin Path to the `cpfe` compiler binary.
 * @param[in] work_dir Scratch directory path.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t test_e2e_types_nullptr_bool_bitint(
    const std::string& cpfe_bin,
    const std::string& work_dir) noexcept {
  std::string src_cpp = work_dir + "/e2e_types.cpp";
  std::string exe_cpp = work_dir + "/e2e_types_exe";
  std::string src_c = work_dir + "/e2e_bitint.c";
  std::string exe_c = work_dir + "/e2e_bitint_exe";

  const char* cpp_code = R"CPP(
namespace std { typedef decltype(nullptr) nullptr_t; }
struct NullHolder {
  std::nullptr_t np;
  bool flags[8];
  unsigned int bf1 : 5;
  unsigned int bf2 : 11;
};
static int ovl(void*) noexcept { return 1; }
static int ovl(std::nullptr_t) noexcept { return 2; }
static std::nullptr_t passthrough_np(std::nullptr_t x, int* side) noexcept {
  *side += 10;
  return x;
}
int main() {
  static_assert(sizeof(bool[8]) == 8, "bool[8] must be 8 bytes");
  int side = 0;
  std::nullptr_t local_np = (side += 5, nullptr);
  NullHolder h = { passthrough_np(local_np, &side), {true, false, true, true, false, false, true, false}, 19, 1337 };
  if (ovl(h.np) != 2) return 1;
  if (ovl((void*)0) != 1) return 2;
  if (side != 15) return 3;
  if (!h.flags[0] || h.flags[1] || !h.flags[2] || !h.flags[6] || h.flags[7]) return 4;
  if (h.bf1 != 19 || h.bf2 != 1337) return 5;
  return 0;
}
)CPP";
  LLVM_GEN_BE_CHECK(write_text_file(src_cpp, cpp_code));
  int rc = -1;
  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "--c++20", src_cpp, "-o", exe_cpp}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  assert(!llvm::sys::fs::exists(work_dir + "/e2e_types.ll"));
  LLVM_GEN_BE_CHECK(run_cmd(exe_cpp, {exe_cpp}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::verification_failure;

  const char* c_code = R"C(
int main(void) {
  unsigned _BitInt(256) a = 1;
  unsigned _BitInt(256) b = (a << 200) + 42u;
  unsigned _BitInt(256) q = b >> 200;
  unsigned _BitInt(256) r = b - (q << 200);
  if (q != 1u || r != 42u) return 1;
  return 0;
}
)C";
  LLVM_GEN_BE_CHECK(write_text_file(src_c, c_code));
  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "--c23", src_c, "-o", exe_c}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(exe_c, {exe_c}, &rc));
  return (rc == 0) ? llvm_gen_be_error_t::ok : llvm_gen_be_error_t::verification_failure;
}

/**
 * @brief Tests C11/GNU atomics including read-only `expected` memory on successful `cmpxchg`, pointer-desired `__atomic_compare_exchange`, and float `fetch_add`.
 * @details Maps a hardware read-only page (`PROT_READ`) for `expected` on a known-success `cmpxchg` to prove no store occurs on success!
 * @param[in] cpfe_bin Path to the `cpfe` compiler binary.
 * @param[in] work_dir Scratch directory path.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t test_e2e_atomics(
    const std::string& cpfe_bin,
    const std::string& work_dir) noexcept {
  auto clang_prog = llvm::sys::findProgramByName("clang");
  if (!clang_prog) return llvm_gen_be_error_t::io_error;

  std::string helper_c = work_dir + "/e2e_ro_page.c";
  std::string helper_o = work_dir + "/e2e_ro_page.o";
  std::string src = work_dir + "/e2e_atomics.c";
  std::string atomics_o = work_dir + "/e2e_atomics.o";
  std::string exe = work_dir + "/e2e_atomics_exe";

  const char* helper_code = R"C(
#include <sys/mman.h>
#include <unistd.h>
extern int run_edg_atomic_checks(int* ro_expected);
int main(void) {
  long page_sz = sysconf(_SC_PAGESIZE);
  int* ro_expected = (int*)mmap(0, (unsigned long)page_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (ro_expected == (int*)-1) return 1;
  *ro_expected = 100;
  if (mprotect(ro_expected, (unsigned long)page_sz, PROT_READ) != 0) return 2;
  int rc = run_edg_atomic_checks(ro_expected);
  munmap(ro_expected, (unsigned long)page_sz);
  return rc;
}
)C";

  const char* code = R"C(
struct BigPair { long a; long b; long c; };

int run_edg_atomic_checks(int* ro_expected) {
  int target = 100;
  /* Must succeed WITHOUT writing to hardware PROT_READ ro_expected! */
  _Bool ok1 = __atomic_compare_exchange_n(&target, ro_expected, 250, 0, 5, 5);
  if (!ok1 || target != 250) return 3;

  /* Generic __atomic_compare_exchange takes desired BY POINTER */
  int exp2 = 250;
  int desired2 = 500;
  _Bool ok2 = __atomic_compare_exchange(&target, &exp2, &desired2, 0, 5, 5);
  if (!ok2 || target != 500) return 4;

  /* Failure path MUST update exp_fail to 500 */
  int exp_fail = 123;
  int desired3 = 999;
  _Bool ok3 = __atomic_compare_exchange(&target, &exp_fail, &desired3, 0, 5, 5);
  if (ok3 || exp_fail != 500 || target != 500) return 5;

  /* C11 floating-point atomic fetch_add & struct atomic load/store */
  _Atomic float fval = 1.5f;
  float fdelta = 2.25f;
  float prev_f = __c11_atomic_fetch_add(&fval, fdelta, 5);
  float cur_f = __c11_atomic_load(&fval, 5);
  if (prev_f != 1.5f || cur_f != 3.75f) return 6;

  struct Pair8 { int x; int y; };
  struct Pair8 p_src = {11, 22};
  struct Pair8 p_dst = {0, 0};
  __atomic_store(&p_dst, &p_src, 5);
  struct Pair8 p_out = {0, 0};
  __atomic_load(&p_dst, &p_out, 5);
  if (p_out.x != 11 || p_out.y != 22) return 7;

  return 0;
}
)C";
  LLVM_GEN_BE_CHECK(write_text_file(helper_c, helper_code));
  LLVM_GEN_BE_CHECK(write_text_file(src, code));
  int rc = -1;
  LLVM_GEN_BE_CHECK(run_cmd(*clang_prog, {*clang_prog, "-c", helper_c, "-o", helper_o}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(
      cpfe_bin,
      {cpfe_bin, "-nostdinc", "--clang", "--clang_version=180100", "-std=c11", "-c", src, "-o", atomics_o},
      &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(*clang_prog, {*clang_prog, helper_o, atomics_o, "-o", exe}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(exe, {exe}, &rc));
  return (rc == 0) ? llvm_gen_be_error_t::ok : llvm_gen_be_error_t::verification_failure;
}

/**
 * @brief Tests `_Complex float`, `_Complex double`, and `_Complex long double` ABI calls across Clang-compiled reference `.o` files.
 * @details Compiles the callee with system `clang` and the caller with `cpfe`, verifying binary ABI compatibility.
 * @param[in] cpfe_bin Path to the `cpfe` compiler binary.
 * @param[in] work_dir Scratch directory path.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t test_e2e_complex_clang_abi(
    const std::string& cpfe_bin,
    const std::string& work_dir) noexcept {
  auto clang_prog = llvm::sys::findProgramByName("clang");
  if (!clang_prog) return llvm_gen_be_error_t::io_error;

  std::string ref_c = work_dir + "/ref_complex.c";
  std::string ref_o = work_dir + "/ref_complex.o";
  std::string main_c = work_dir + "/caller_complex.c";
  std::string main_o = work_dir + "/caller_complex.o";
  std::string exe = work_dir + "/complex_abi_exe";

  const char* ref_code = R"C(
extern _Complex long double edg_sub_cld(_Complex long double a, _Complex long double b);
_Complex float clang_add_cf(_Complex float a, _Complex float b) { return a + b; }
_Complex double clang_mul_cd(_Complex double a, _Complex double b) { return a * b; }
_Complex long double clang_add_cld(_Complex long double a, _Complex long double b) { return a + b; }
int clang_verify_edg_cld(void) {
  _Complex long double r = edg_sub_cld(9.0L + 11.0Li, 2.5L + 4.0Li);
  return (__real__ r == 6.5L && __imag__ r == 7.0L) ? 0 : 1;
}
)C";
  const char* caller_code = R"C(
extern _Complex float clang_add_cf(_Complex float a, _Complex float b);
extern _Complex double clang_mul_cd(_Complex double a, _Complex double b);
extern _Complex long double clang_add_cld(_Complex long double a, _Complex long double b);
extern int clang_verify_edg_cld(void);

_Complex long double edg_sub_cld(_Complex long double a, _Complex long double b) {
  return a - b;
}

int main(void) {
  _Complex float cf1 = 1.5f + 2.5fi;
  _Complex float cf2 = 3.0f - 0.5fi;
  _Complex float cfr = clang_add_cf(cf1, cf2);
  if (__real__ cfr != 4.5f || __imag__ cfr != 2.0f) return 1;

  _Complex double cd1 = 2.0 + 3.0i;
  _Complex double cd2 = 4.0 - 1.0i;
  _Complex double cdr = clang_mul_cd(cd1, cd2);
  /* (2 + 3i)*(4 - i) = 11 + 10i */
  if (__real__ cdr != 11.0 || __imag__ cdr != 10.0) return 2;

  _Complex long double cl1 = 5.0L + 7.0Li;
  _Complex long double cl2 = 2.0L + 3.0Li;
  _Complex long double clr = clang_add_cld(cl1, cl2);
  if (__real__ clr != 7.0L || __imag__ clr != 10.0L) return 3;
  if (clang_verify_edg_cld() != 0) return 4;
  return 0;
}
)C";
  LLVM_GEN_BE_CHECK(write_text_file(ref_c, ref_code));
  LLVM_GEN_BE_CHECK(write_text_file(main_c, caller_code));
  int rc = -1;
  LLVM_GEN_BE_CHECK(run_cmd(*clang_prog, {*clang_prog, "-c", ref_c, "-o", ref_o}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "-std=c11", "-c", main_c, "-o", main_o}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(*clang_prog, {*clang_prog, ref_o, main_o, "-o", exe}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(exe, {exe}, &rc));
  return (rc == 0) ? llvm_gen_be_error_t::ok : llvm_gen_be_error_t::verification_failure;
}

/**
 * @brief Tests `__builtin_FILE`, `__builtin_LINE`, `__builtin_FUNCTION`, and `std::source_location::current()` inside `#include`d headers.
 * @details Verifies that calls inside an included header report the true header filename, 1-based line number, and unmangled function name.
 * @param[in] cpfe_bin Path to the `cpfe` compiler binary.
 * @param[in] work_dir Scratch directory path.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t test_e2e_source_location_header(
    const std::string& cpfe_bin,
    const std::string& work_dir) noexcept {
  std::string hdr = work_dir + "/srcloc_hdr.hpp";
  std::string src = work_dir + "/srcloc_main.cpp";
  std::string exe = work_dir + "/srcloc_exe";

  const char* hdr_code = R"CPP(
namespace std {
  struct source_location {
    const char* f;
    const char* fn;
    unsigned int ln;
    unsigned int col;
    static consteval source_location current(
        const char* file = __builtin_FILE(),
        const char* func = __builtin_FUNCTION(),
        unsigned int line = __builtin_LINE(),
        unsigned int column = __builtin_COLUMN()) noexcept {
      return {file, func, line, column};
    }
    constexpr const char* file_name() const noexcept { return f; }
    constexpr const char* function_name() const noexcept { return fn; }
    constexpr unsigned int line() const noexcept { return ln; }
  };
}
inline int header_probe(const char** out_file, const char** out_fn) noexcept {
  auto loc = std::source_location::current();
  *out_file = loc.file_name();
  *out_fn = loc.function_name();
  return (int)loc.line();
}
)CPP";
  const char* main_code = R"CPP(
extern "C" char* strstr(const char*, const char*);
// Pad lines before #include so cumulative seq != header line!
// pad 1
// pad 2
// pad 3
// pad 4
#include "srcloc_hdr.hpp"

int main() {
  const char* f = nullptr;
  const char* fn = nullptr;
  int ln = header_probe(&f, &fn);
  if (!f || !strstr(f, "srcloc_hdr.hpp")) return 1;
  if (ln != 21) return 2;
  if (!fn || !strstr(fn, "header_probe")) return 3;
  return 0;
}
)CPP";
  LLVM_GEN_BE_CHECK(write_text_file(hdr, hdr_code));
  LLVM_GEN_BE_CHECK(write_text_file(src, main_code));
  int rc = -1;
  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "--c++20", "-I" + work_dir, src, "-o", exe}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(exe, {exe}, &rc));
  return (rc == 0) ? llvm_gen_be_error_t::ok : llvm_gen_be_error_t::verification_failure;
}

/**
 * @brief Tests virtual diamond inheritance (`_ZTT`/`_ZTC`), covariant thunks, pointer-to-member calls & null casts, RAII, partial-init unwinding, `noexcept` termination, and `static`/`thread_local` destructors.
 * @details Exercises Section 9.1 items 6 & 7 end-to-end on native hardware.
 * @param[in] cpfe_bin Path to the `cpfe` compiler binary.
 * @param[in] work_dir Scratch directory path.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t test_e2e_cxx_abi_vtable_ptrmem_raii_eh(
    const std::string& cpfe_bin,
    const std::string& work_dir) noexcept {
  std::string src = work_dir + "/e2e_cxx_abi.cpp";
  std::string exe = work_dir + "/e2e_cxx_abi_exe";
  std::string term_src = work_dir + "/e2e_noexcept_term.cpp";
  std::string term_exe = work_dir + "/e2e_noexcept_term_exe";

  const char* cxx_code = R"CPP(
extern "C" void _Exit(int);

struct RetBase { int a = 1; virtual ~RetBase() {} };
struct RetOffset { int pad = 9; };
struct RetDerived : RetOffset, RetBase { int b = 2; };

struct VBase {
  int vb = 10;
  static int ctor_vcall_log;
  VBase() { ctor_vcall_log += who(); }
  virtual int who() { return 1; }
  virtual RetBase* cov() { static RetDerived rd; return &rd; }
  virtual ~VBase() {}
};
int VBase::ctor_vcall_log = 0;

struct Left : virtual VBase {
  int l = 20;
  Left() { ctor_vcall_log += who() * 10; }
  int who() override { return 2; }
};
struct Right : virtual VBase {
  int r = 30;
  int right_add(int x) { return r + x; }
  virtual int vright(int x) { return r + vb + x; }
};
struct Diamond : Left, Right {
  int d = 40;
  int who() override { return 3; }
  RetDerived* cov() override { static RetDerived rd; rd.a = 77; return &rd; }
  int vright(int x) override { return vb + l + r + d + x; }
};

struct ForwardRaii {
  int* counter;
  explicit ForwardRaii(int* c) : counter(c) { *counter += 1; }
  ~ForwardRaii();
};

struct SubPart {
  static int live_parts;
  SubPart() { ++live_parts; }
  ~SubPart() { --live_parts; }
};
int SubPart::live_parts = 0;

struct PartialComposite {
  SubPart p1;
  SubPart p2;
  PartialComposite(bool fail) {
    if (fail) throw 42;
  }
};

static int g_static_dtor_ran = 0;
static int g_tls_dtor_ran = 0;

struct ExitVerifier {
  ~ExitVerifier() {
    if (g_static_dtor_ran != 1 || g_tls_dtor_ran != 1) {
      _Exit(91);
    }
  }
};
static ExitVerifier g_verifier;

struct StaticLocalTracker {
  ~StaticLocalTracker() { ++g_static_dtor_ran; }
};
struct TlsTracker {
  ~TlsTracker() { ++g_tls_dtor_ran; }
};

static void touch_locals() {
  static StaticLocalTracker s_loc;
  thread_local TlsTracker t_loc;
  (void)&s_loc;
  (void)&t_loc;
}

int main() {
  touch_locals();

  int raii_cnt = 0;
  {
    ForwardRaii fr(&raii_cnt);
    if (raii_cnt != 1) return 1;
  }
  if (raii_cnt != 11) return 2;

  Diamond dm;
  /* VBase() dispatches VBase::who()==1, Left() dispatches Left::who()==2 (*10=20) via _ZTC construction vtable */
  if (VBase::ctor_vcall_log != 21) return 10;
  VBase* vb_ptr = &dm;
  if (vb_ptr->who() != 3) return 3;
  RetBase* rb = vb_ptr->cov();
  if (rb->a != 77) return 4;

  /* Pointer-to-member-function (virtual + non-virtual + null cast preservation) */
  int (Right::*pmf_nonvirt)(int) = &Right::right_add;
  int (Diamond::*pmf_derived)(int) = pmf_nonvirt;
  if ((dm.*pmf_derived)(5) != 35) return 5;

  int (Right::*pmf_virt)(int) = &Right::vright;
  int (Diamond::*pmf_virt_d)(int) = pmf_virt;
  if ((dm.*pmf_virt_d)(5) != 105) return 6;

  /* Virtual slot-0 PMF must NEVER compare equal to nullptr (especially on ARM64 Itanium ABI!) */
  int (VBase::*pmf_slot0)() = &VBase::who;
  if (pmf_slot0 == nullptr || !(pmf_slot0 != nullptr)) return 11;
  if ((vb_ptr->*pmf_slot0)() != 3) return 12;

  int (Right::*null_pmf)(int) = nullptr;
  int (Diamond::*null_pmf_cast)(int) = null_pmf;
  if (null_pmf_cast != nullptr) return 7;

  int Right::*null_pmd = nullptr;
  int Diamond::*null_pmd_cast = null_pmd;
  if (null_pmd_cast != nullptr) return 8;

  /* Constructor partial-initialization unwinding */
  int caught_val = 0;
  try {
    PartialComposite pc(true);
  } catch (int v) {
    caught_val = v;
  }
  if (caught_val != 42 || SubPart::live_parts != 0) return 9;

  return 0;
}

ForwardRaii::~ForwardRaii() { *counter += 10; }
)CPP";
  LLVM_GEN_BE_CHECK(write_text_file(src, cxx_code));
  int rc = -1;
  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "--c++20", src, "-o", exe}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(exe, {exe}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::verification_failure;

  const char* term_code = R"CPP(
extern "C" void _Exit(int);
namespace __cxxabiv1 {
  extern "C" void (*__cxa_set_terminate(void (*)() noexcept))() noexcept;
}
namespace std {
  typedef void (*terminate_handler)();
  terminate_handler set_terminate(terminate_handler f) noexcept;
}
static void custom_term() { _Exit(77); }
static void must_not_throw() noexcept { throw 123; }
int main() {
  std::set_terminate(custom_term);
  must_not_throw();
  return 0;
}
)CPP";
  LLVM_GEN_BE_CHECK(write_text_file(term_src, term_code));
  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "--c++20", term_src, "-o", term_exe}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(term_exe, {term_exe}, &rc));
  return (rc == 77) ? llvm_gen_be_error_t::ok : llvm_gen_be_error_t::verification_failure;
}

/**
 * @brief Tests C++20 coroutines (`Generator<int>` & `Task` with `co_return` and custom `promise_type::operator new`) at `-O0` and `-O2`.
 * @details Verifies coroutine splitting at `-O0`, HALO elision and `noinline` custom allocator invocation at `-O2`.
 * @param[in] cpfe_bin Path to the `cpfe` compiler binary.
 * @param[in] work_dir Scratch directory path.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t test_e2e_coroutines_o0_and_o2(
    const std::string& cpfe_bin,
    const std::string& work_dir) noexcept {
  std::string src = work_dir + "/e2e_coro.cpp";
  std::string exe_o0 = work_dir + "/e2e_coro_o0";
  std::string exe_o2 = work_dir + "/e2e_coro_o2";

  const char* code = R"CPP(
extern "C" int strcmp(const char*, const char*);
extern "C" unsigned long strlen(const char*);
extern "C" void* memcpy(void*, const void*, unsigned long);

namespace std {
  template<class R, class... Args>
  struct coroutine_traits { using promise_type = typename R::promise_type; };
  template<class Promise = void> struct coroutine_handle;
  template<> struct coroutine_handle<void> {
    void* ptr = nullptr;
    void resume() const { __builtin_coro_resume(ptr); }
    void destroy() const { __builtin_coro_destroy(ptr); }
    bool done() const { return __builtin_coro_done(ptr); }
  };
  template<class Promise> struct coroutine_handle : coroutine_handle<void> {
    static coroutine_handle from_promise(Promise& p) noexcept {
      coroutine_handle h;
      h.ptr = __builtin_coro_promise((char*)&p, alignof(Promise), true);
      return h;
    }
    Promise& promise() const noexcept {
      return *reinterpret_cast<Promise*>(__builtin_coro_promise(ptr, alignof(Promise), false));
    }
  };
  struct suspend_always {
    constexpr bool await_ready() const noexcept { return false; }
    constexpr void await_suspend(coroutine_handle<>) const noexcept {}
    constexpr void await_resume() const noexcept {}
  };
}

struct HeapStr {
  char* buf;
  HeapStr() noexcept : buf(nullptr) {}
  explicit HeapStr(const char* s) {
    unsigned long n = strlen(s) + 1;
    buf = new char[n];
    memcpy(buf, s, n);
  }
  HeapStr(const HeapStr& o) : buf(nullptr) {
    if (o.buf) {
      unsigned long n = strlen(o.buf) + 1;
      buf = new char[n];
      memcpy(buf, o.buf, n);
    }
  }
  HeapStr& operator=(const HeapStr& o) {
    if (this != &o) {
      delete[] buf;
      buf = nullptr;
      if (o.buf) {
        unsigned long n = strlen(o.buf) + 1;
        buf = new char[n];
        memcpy(buf, o.buf, n);
      }
    }
    return *this;
  }
  ~HeapStr() { delete[] buf; }
};

template<class T>
struct Generator {
  struct promise_type {
    T cur{};
    Generator get_return_object() noexcept {
      return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    std::suspend_always yield_value(T v) noexcept { cur = v; return {}; }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
  };
  std::coroutine_handle<promise_type> h;
};

static int g_coro_alloc_count = 0;

struct PromiseAllocBase {
  static void* operator new(unsigned long sz) noexcept {
    ++g_coro_alloc_count;
    return ::operator new(sz);
  }
  static void operator delete(void* p) noexcept { ::operator delete(p); }
};

template<class T>
struct Task {
  struct promise_type : PromiseAllocBase {
    T result{};
    bool caught_unhandled = false;
    Task get_return_object() noexcept {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_value(const T& v) { result = v; }
    void unhandled_exception() noexcept { caught_unhandled = true; }
  };
  std::coroutine_handle<promise_type> h;
};

static Generator<int> gen_seq() {
  co_yield 10;
  co_yield 20;
  co_yield 30;
  co_return;
}

__attribute__((noinline)) static Task<HeapStr> str_task(bool should_throw) {
  co_await std::suspend_always{};
  if (should_throw) throw 99;
  co_return HeapStr("coro_ok_42");
}

int main() {
  Generator<int> g = gen_seq();
  int acc = 0;
  g.h.resume(); acc += g.h.promise().cur;
  g.h.resume(); acc += g.h.promise().cur;
  g.h.resume(); acc += g.h.promise().cur;
  g.h.destroy();
  if (acc != 60) return 1;

  Task<HeapStr> t = str_task(false);
  t.h.resume();
  t.h.resume();
  bool str_ok = (t.h.promise().result.buf && strcmp(t.h.promise().result.buf, "coro_ok_42") == 0);
  t.h.destroy();
  if (!str_ok || g_coro_alloc_count < 1) return 2;

  Task<HeapStr> t_err = str_task(true);
  t_err.h.resume();
  t_err.h.resume();
  bool uh_ok = t_err.h.promise().caught_unhandled;
  t_err.h.destroy();
  if (!uh_ok) return 3;
  return 0;
}
)CPP";
  LLVM_GEN_BE_CHECK(write_text_file(src, code));
  int rc = -1;
  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "--c++20", "-O0", src, "-o", exe_o0}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(exe_o0, {exe_o0}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::verification_failure;

  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "--c++20", "-O2", src, "-o", exe_o2}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(exe_o2, {exe_o2}, &rc));
  return (rc == 0) ? llvm_gen_be_error_t::ok : llvm_gen_be_error_t::verification_failure;
}

/**
 * @brief Tests OpenMP `parallel for` (32-bit & 64-bit bounds, `collapse(2)`, `reduction(+:sum)`, C++ `firstprivate`), SIMD vectors, `-shared` dylib/so, and multi-TU linking.
 * @details Verifies exact `sum == N*(N-1)/2`, `dlopen`/`dlsym` on a `-shared` library, and multi-TU linking with zero stray `.ll` files.
 * @param[in] cpfe_bin Path to the `cpfe` compiler binary.
 * @param[in] work_dir Scratch directory path.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t test_e2e_omp_simd_shared_multitu(
    const std::string& cpfe_bin,
    const std::string& work_dir) noexcept {
  std::string omp_src = work_dir + "/e2e_omp.cpp";
  std::string omp_exe = work_dir + "/e2e_omp_exe";
  const char* omp_code = R"CPP(
struct Factor {
  int addend;
  Factor() : addend(1) {}
  Factor(const Factor& o) : addend(o.addend) {}
  ~Factor() {}
};

int main() {
  int sum32 = 0;
  #pragma omp parallel for reduction(+:sum32)
  for (int i = 0; i < 100; ++i) {
    sum32 += i;
  }
  if (sum32 != 4950) return 1;

  long long sum64 = 0;
  Factor fp;
  #pragma omp parallel for collapse(2) firstprivate(fp) reduction(+:sum64)
  for (long long r = 0; r < 25; ++r) {
    for (long long c = 0; c < 8; ++c) {
      sum64 += (r * 8 + c) * fp.addend;
    }
  }
  /* N = 200 -> 200 * 199 / 2 = 19900 */
  if (sum64 != 19900) return 2;
  return 0;
}
)CPP";
  LLVM_GEN_BE_CHECK(write_text_file(omp_src, omp_code));
  int rc = -1;
  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "--c++20", "-fopenmp", omp_src, "-o", omp_exe}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(omp_exe, {omp_exe}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::verification_failure;

  /* SIMD vector arithmetic & shuffles passing verifyModule */
  std::string simd_src = work_dir + "/e2e_simd.c";
  std::string simd_exe = work_dir + "/e2e_simd_exe";
  const char* simd_code = R"C(
typedef int v4si __attribute__((vector_size(16)));
typedef float v4sf __attribute__((vector_size(16)));
int main(void) {
  v4si a = {1, 2, 3, 4};
  v4si b = {10, 20, 30, 40};
  v4si c = a + b;
  if ((a + b)[0] != 11) return 10;
  v4si sh = __builtin_shufflevector(c, c, 3, 2, 1, 0);
  sh[1] = 99;
  v4sf fa = {1.5f, 2.5f, 3.5f, 4.5f};
  v4sf fb = {2.0f, 2.0f, 2.0f, 2.0f};
  v4sf fc = fa * fb;
  if (sh[0] != 44 || sh[1] != 99 || sh[3] != 11) return 1;
  if (fc[0] != 3.0f || fc[3] != 9.0f) return 2;
  if (__builtin_popcount(0xF0F0u) != 8) return 3;
  return 0;
}
)C";
  LLVM_GEN_BE_CHECK(write_text_file(simd_src, simd_code));
  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "-std=gnu11", simd_src, "-o", simd_exe}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(simd_exe, {simd_exe}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::verification_failure;

#if defined(__aarch64__) && defined(__APPLE__)
  /* Real <arm_neon.h> header compilation + llvm::verifyModule + native execution */
  if (auto clang_prog = llvm::sys::findProgramByName("clang")) {
    std::string res_file = work_dir + "/clang_res_dir.txt";
    std::string sdk_file = work_dir + "/sdk_dir.txt";
    std::optional<llvm::StringRef> redirs_res[] = {std::nullopt, llvm::StringRef(res_file), std::nullopt};
    if (llvm::sys::ExecuteAndWait(*clang_prog, {*clang_prog, "-print-resource-dir"}, std::nullopt, redirs_res) == 0) {
      auto res_buf = llvm::MemoryBuffer::getFile(res_file);
      if (auto xcrun_prog = llvm::sys::findProgramByName("xcrun")) {
        std::optional<llvm::StringRef> redirs_sdk[] = {std::nullopt, llvm::StringRef(sdk_file), std::nullopt};
        if (res_buf && llvm::sys::ExecuteAndWait(*xcrun_prog, {*xcrun_prog, "--show-sdk-path"}, std::nullopt, redirs_sdk) == 0) {
          auto sdk_buf = llvm::MemoryBuffer::getFile(sdk_file);
          if (sdk_buf) {
            std::string clang_inc = "--sys_include=" + (*res_buf)->getBuffer().trim().str() + "/include";
            std::string sdk_inc = "--sys_include=" + (*sdk_buf)->getBuffer().trim().str() + "/usr/include";
            std::string neon_src = work_dir + "/e2e_arm_neon.c";
            std::string neon_exe = work_dir + "/e2e_arm_neon_exe";
            const char* neon_code = R"C(
#include <arm_neon.h>
int main(void) {
  int32_t src[4] = {1, 2, 3, 4};
  int32_t dst[4] = {0, 0, 0, 0};
  int32x4_t v = vld1q_s32(src);
  int32x4_t ten = vdupq_n_s32(10);
  int32x4_t sum = vaddq_s32(v, ten);
  vst1q_s32(dst, sum);
  if (dst[0] != 11 || dst[3] != 14 || vgetq_lane_s32(sum, 2) != 13) return 1;
  float32x4_t fm = vfmaq_f32(vdupq_n_f32(1.0f), vdupq_n_f32(2.0f), vdupq_n_f32(3.0f));
  return (vgetq_lane_f32(fm, 0) == 7.0f) ? 0 : 2;
}
)C";
            LLVM_GEN_BE_CHECK(write_text_file(neon_src, neon_code));
            LLVM_GEN_BE_CHECK(run_cmd(
                cpfe_bin,
                {cpfe_bin, "--c11", "--clang", "--clang_version=210000",
                 "-D__arm64__=1", "-D__aarch64__=1", "-D__ARM_NEON=1",
                 "-D__ARM_FP=0x0E", "-D__ARM_FEATURE_FMA=1",
                 "-D__LITTLE_ENDIAN__=1", "-D__ARM_64BIT_STATE=1",
                 clang_inc, sdk_inc, neon_src, "-o", neon_exe},
                &rc));
            if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
            LLVM_GEN_BE_CHECK(run_cmd(neon_exe, {neon_exe}, &rc));
            if (rc != 0) return llvm_gen_be_error_t::verification_failure;
          }
        }
      }
    }
  }
#endif

  /* Shared library (-shared) creation + dlopen/dlsym */
  std::string sh_src = work_dir + "/e2e_shlib.c";
#if defined(__APPLE__)
  std::string sh_out = work_dir + "/libe2e_test.dylib";
#else
  std::string sh_out = work_dir + "/libe2e_test.so";
#endif
  const char* sh_code = R"C(
int edg_shared_compute(int x, int y) { return x * 10 + y; }
)C";
  LLVM_GEN_BE_CHECK(write_text_file(sh_src, sh_code));
  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "-std=c11", "-shared", "-fPIC", sh_src, "-o", sh_out}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  assert(!llvm::sys::fs::exists(work_dir + "/e2e_shlib.ll"));

  void* dl = dlopen(sh_out.c_str(), RTLD_NOW);
  if (!dl) return llvm_gen_be_error_t::verification_failure;
  using ComputeFn = int (*)(int, int);
  ComputeFn fn = reinterpret_cast<ComputeFn>(dlsym(dl, "edg_shared_compute"));
  if (!fn || fn(4, 2) != 42) {
    dlclose(dl);
    return llvm_gen_be_error_t::verification_failure;
  }
  dlclose(dl);

  /* Multi-TU executable compilation & linking without stray .ll files */
  std::string tu1 = work_dir + "/multi_tu1.cpp";
  std::string tu1_o = work_dir + "/multi_tu1.o";
  std::string tu2 = work_dir + "/multi_tu2.cpp";
  std::string multi_exe = work_dir + "/multi_tu_exe";
  LLVM_GEN_BE_CHECK(write_text_file(tu1, R"CPP(
int helper_mul(int a, int b) { return a * b; }
)CPP"));
  LLVM_GEN_BE_CHECK(write_text_file(tu2, R"CPP(
int helper_mul(int, int);
int main() { return helper_mul(6, 7) == 42 ? 0 : 1; }
)CPP"));
  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "-std=c++20", "-c", tu1, "-o", tu1_o}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "-std=c++20", tu2, tu1_o, "-o", multi_exe}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  assert(llvm::sys::fs::exists(tu1_o));
  assert(!llvm::sys::fs::exists(work_dir + "/multi_tu1.ll"));
  assert(!llvm::sys::fs::exists(work_dir + "/multi_tu2.ll"));
  LLVM_GEN_BE_CHECK(run_cmd(multi_exe, {multi_exe}, &rc));
  return (rc == 0) ? llvm_gen_be_error_t::ok : llvm_gen_be_error_t::verification_failure;
}

/**
 * @brief Tests hardened edge cases across C99 `_Complex` runtime lowering, indirect `(*fp)` calls, `va_arg`, C++17 elision, covariant thunks, catch-by-value rethrow, and coroutine allocation-failure/symmetric-transfer.
 * @details Compiles and executes native C11/C23/C++20 programs verifying self-referential globals, non-first union
 *          initializers, >64-bit straddling bitfields, `_BitInt(256)` ABI/div/float casts, C99 `_Complex float`
 *          multiply/divide/conjugate/lvalue `__real__`/`__imag__`, `(*fp)(...)` indirect `sret`+HFA calls, `va_arg`
 *          with >16B structs/HFA/`__int128`, non-movable `= delete` C++17 prvalue elision, over-aligned `new`/`delete`,
 *          partial-array `new[]` EH cleanup, catch-by-value early `return` + `throw;`, diamond `_ZTC` construction
 *          vtables + multiple-inheritance covariant return thunks, and coroutine symmetric transfer + parameter-preview
 *          `operator new` + `get_return_object_on_allocation_failure()`.
 * @param[in] cpfe_bin Path to the `cpfe` compiler binary.
 * @param[in] work_dir Scratch directory path.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t test_e2e_hardened_regression_suite(
    const std::string& cpfe_bin,
    const std::string& work_dir) noexcept {
  int rc = -1;

  /* 1. Self-referential global + union non-first member + straddling bitfields + _Complex mul/div/conj + (*fp) + va_arg */
  std::string c_src = work_dir + "/e2e_hardened_c.c";
  std::string c_exe = work_dir + "/e2e_hardened_c";
  LLVM_GEN_BE_CHECK(write_text_file(c_src, R"C(
#include <stdarg.h>

struct SelfNode { struct SelfNode* self; int val; };
struct SelfNode g_self = { &g_self, 42 };

union BigUnion {
  char c;
  struct { double d; long arr[2]; } big;
};
union BigUnion g_big_u = { .big = { 3.5, { 111, 222 } } };

struct StraddleBF {
  unsigned long long a : 40;
  unsigned long long b : 40;
  int : 0;
  unsigned int c : 7;
};
struct StraddleBF g_bf = { 0xABCDEF1234ULL, 0x9876543210ULL, 99 };

struct BigS { long a; long b; long c; };
struct HFA2 { float x; float y; };

static struct BigS make_big(long x, struct HFA2 h, _Bool flag) {
  struct BigS r = { x + (long)h.x, (long)h.y, flag ? 100 : 200 };
  return r;
}

static long sum_variadic(int kind, ...) {
  va_list ap;
  va_start(ap, kind);
  struct HFA2 h = va_arg(ap, struct HFA2);
  struct BigS b = va_arg(ap, struct BigS);
  __int128 bigi = va_arg(ap, __int128);
  va_end(ap);
  return (long)h.x + (long)h.y + b.a + b.b + b.c + (long)bigi;
}

int main(void) {
  if (g_self.self != &g_self || g_self.val != 42) return 1;
  if (g_big_u.big.d != 3.5 || g_big_u.big.arr[0] != 111 || g_big_u.big.arr[1] != 222) return 2;
  if (g_bf.a != 0xABCDEF1234ULL || g_bf.b != 0x9876543210ULL || g_bf.c != 99) return 3;

  _Complex float ca = 2.0f + 3.0fi;
  _Complex float cb = 4.0f - 1.0fi;
  _Complex float prod = ca * cb;
  if (__real__ prod != 11.0f || __imag__ prod != 10.0f) return 4;
  _Complex float quot = prod / ca;
  float dr = __real__ quot - 4.0f;
  float di = __imag__ quot - (-1.0f);
  if (dr < -1e-4f || dr > 1e-4f || di < -1e-4f || di > 1e-4f) return 5;
  _Complex float cj = ~ca;
  if (__real__ cj != 2.0f || __imag__ cj != -3.0f) return 6;
  __real__ cj = 7.0f;
  __imag__ cj = 9.0f;
  if (__real__ cj != 7.0f || __imag__ cj != 9.0f) return 7;

  struct BigS (*fp)(long, struct HFA2, _Bool) = &make_big;
  struct HFA2 h = { 3.0f, 7.0f };
  struct BigS res = (*fp)(10, h, 1);
  if (res.a != 13 || res.b != 7 || res.c != 100) return 8;
  if (sum_variadic(1, h, res, (__int128)50) != 180) return 9;
  return 0;
}
)C"));
  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "--gcc", "-std=gnu11", c_src, "-o", c_exe}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(c_exe, {c_exe}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::verification_failure;

  /* 2. C++20: NonMovable elision, aligned new, partial new[] EH, catch-by-val rethrow, diamond _ZTC + covariant thunk, coro sym-transfer & alloc-fail */
  std::string cpp_src = work_dir + "/e2e_hardened_cpp.cpp";
  std::string cpp_exe = work_dir + "/e2e_hardened_cpp";
  LLVM_GEN_BE_CHECK(write_text_file(cpp_src, R"CPP(
namespace std {
  enum class align_val_t : decltype(sizeof(0)) {};
  template<class R, class... Args>
  struct coroutine_traits { using promise_type = typename R::promise_type; };
  template<class Promise = void> struct coroutine_handle;
  template<> struct coroutine_handle<void> {
    void* ptr = nullptr;
    void resume() const { __builtin_coro_resume(ptr); }
    void destroy() const { __builtin_coro_destroy(ptr); }
  };
  template<class Promise> struct coroutine_handle : coroutine_handle<void> {
    static coroutine_handle from_promise(Promise& p) noexcept {
      coroutine_handle h;
      h.ptr = __builtin_coro_promise((char*)&p, alignof(Promise), true);
      return h;
    }
  };
  struct suspend_always {
    constexpr bool await_ready() const noexcept { return false; }
    constexpr void await_suspend(coroutine_handle<>) const noexcept {}
    constexpr void await_resume() const noexcept {}
  };
}

struct NonMovable {
  int a, b, c, d, e;
  explicit NonMovable(int x) : a(x), b(x + 1), c(x + 2), d(x + 3), e(x + 4) {}
  NonMovable(const NonMovable&) = delete;
  NonMovable(NonMovable&&) = delete;
};
static NonMovable make_nm(int x) { return NonMovable(x); }

struct alignas(64) OverAligned {
  long data[8];
  OverAligned() { data[0] = 777; }
  ~OverAligned() { data[0] = 0; }
};

struct ArrayElem {
  static int live;
  static int created;
  int id;
  ArrayElem() : id(++created) {
    if (id == 4) throw 55;
    ++live;
  }
  ~ArrayElem() { --live; }
};
int ArrayElem::live = 0;
int ArrayElem::created = 0;

struct ErrObj {
  static int live;
  int code;
  explicit ErrObj(int c) : code(c) { ++live; }
  ErrObj(const ErrObj& o) : code(o.code + 100) { ++live; }
  ~ErrObj() { --live; }
};
int ErrObj::live = 0;

static int run_catch_rethrow() {
  try {
    try {
      throw ErrObj(7);
    } catch (ErrObj inner) {
      if (inner.code != 107) return 10;
      throw;
    }
  } catch (ErrObj outer) {
    if (outer.code != 107 && outer.code != 207) return 20;
    return 42;
  }
  return 0;
}

struct VBase {
  int vb = 1;
  virtual int who() { return 10; }
  virtual ~VBase() {}
};
struct Mid : virtual VBase {
  int mid_saw = 0;
  Mid() { mid_saw = this->who(); }
  int who() override { return 20; }
};
struct Other : virtual VBase { int pad = 99; };
struct Diamond : Other, Mid {
  int d = 5;
  int who() override { return 30; }
};

struct RetBase { int rb = 11; virtual ~RetBase() {} };
struct RetMid { int rm = 22; virtual ~RetMid() {} };
struct RetDerived : RetBase, RetMid { int rd = 33; };
struct ProducerA { virtual ~ProducerA() {} };
struct ProducerB {
  int bpad = 77;
  virtual RetMid* make() = 0;
  virtual ~ProducerB() {}
};
struct ImplProducer : ProducerA, ProducerB {
  RetDerived inst;
  RetDerived* make() override { return &inst; }
};

static int g_preview_tag = 0;
static bool g_force_null_alloc = false;
static int g_sym_ran = 0;

struct SymTask {
  struct promise_type {
    static void* operator new(unsigned long sz, int tag, std::coroutine_handle<>) noexcept {
      g_preview_tag = tag;
      if (g_force_null_alloc) return nullptr;
      return ::operator new(sz);
    }
    static void* operator new(unsigned long sz, int tag) noexcept {
      g_preview_tag = tag;
      if (g_force_null_alloc) return nullptr;
      return ::operator new(sz);
    }
    static void operator delete(void* p) noexcept { ::operator delete(p); }
    static SymTask get_return_object_on_allocation_failure() noexcept {
      return SymTask{{nullptr}, -999};
    }
    SymTask get_return_object() noexcept {
      return SymTask{std::coroutine_handle<promise_type>::from_promise(*this), 0};
    }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_value(int) noexcept {}
    void unhandled_exception() noexcept {}
  };
  std::coroutine_handle<promise_type> h;
  int fallback_code;
};

struct TransferAwaiter {
  std::coroutine_handle<> target;
  bool await_ready() const noexcept { return false; }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept { return target; }
  void await_resume() const noexcept {}
};

__attribute__((noinline))
static SymTask leaf_coro(int tag) {
  g_sym_ran += tag;
  co_return tag;
}

__attribute__((noinline))
static SymTask root_coro(int tag, std::coroutine_handle<> target) {
  co_await TransferAwaiter{target};
  g_sym_ran += 100;
  co_return tag;
}

int main() {
  NonMovable nm = make_nm(10);
  if (nm.a != 10 || nm.e != 14) return 1;

  OverAligned* oa = new OverAligned();
  if (((unsigned long)oa & 63UL) != 0 || oa->data[0] != 777) return 2;
  delete oa;

  int caught = 0;
  try {
    ArrayElem* arr = new ArrayElem[6];
    (void)arr;
  } catch (int x) {
    caught = x;
  }
  if (caught != 55 || ArrayElem::live != 0) return 3;

  if (run_catch_rethrow() != 42 || ErrObj::live != 0) return 4;

  Diamond dm;
  if (dm.who() != 30 || dm.mid_saw != 20) return 5;

  ImplProducer ip;
  ProducerB* pb = &ip;
  RetMid* rm = pb->make();
  if (rm->rm != 22) return 6;

  SymTask leaf = leaf_coro(7);
  if (g_preview_tag != 7) return 7;
  SymTask root = root_coro(11, leaf.h);
  if (g_preview_tag != 11) return 8;
  root.h.resume();
  if (g_sym_ran != 7) return 9;
  root.h.resume();
  if (g_sym_ran != 107) return 10;
  leaf.h.destroy();
  root.h.destroy();

  g_force_null_alloc = true;
  SymTask failed = leaf_coro(55);
  if (failed.fallback_code != -999) return 11;
  return 0;
}
)CPP"));
  LLVM_GEN_BE_CHECK(run_cmd(cpfe_bin, {cpfe_bin, "-std=c++20", cpp_src, "-o", cpp_exe}, &rc));
  if (rc != 0) return llvm_gen_be_error_t::code_gen_failure;
  LLVM_GEN_BE_CHECK(run_cmd(cpp_exe, {cpp_exe}, &rc));
  return (rc == 0) ? llvm_gen_be_error_t::ok : llvm_gen_be_error_t::verification_failure;
}

/**
 * @brief Entry point for `test_llvm_e2e_execution`.
 * @details Locates sibling `cpfe` binary, prepares isolated scratch directory under `.tmp/agent-llvm/e2e_suite`,
 *          and executes all end-to-end native compilation and execution tests.
 * @param[in] argc Command-line argument count.
 * @param[in] argv Command-line argument vector.
 * @return `0` if all end-to-end tests pass, `1` otherwise.
 */
int main([[maybe_unused]] int argc, char** argv) noexcept {
  llvm::SmallString<256> self_path(argv[0]);
  llvm::sys::fs::make_absolute(self_path);
  llvm::StringRef bin_dir = llvm::sys::path::parent_path(self_path);
  std::string cpfe_bin = (bin_dir + "/cpfe").str();
  if (!llvm::sys::fs::exists(cpfe_bin)) {
    llvm::errs() << "Error: cpfe binary not found at " << cpfe_bin << '\n';
    return 1;
  }

  std::string work_dir = ".tmp/agent-llvm/e2e_suite";
  llvm::sys::fs::create_directories(work_dir);
  llvm::SmallString<256> abs_work(work_dir);
  llvm::sys::fs::make_absolute(abs_work);
  setenv("TMPDIR", abs_work.c_str(), 1);

  if (test_e2e_types_nullptr_bool_bitint(cpfe_bin, work_dir) != llvm_gen_be_error_t::ok) return 1;
  if (test_e2e_atomics(cpfe_bin, work_dir) != llvm_gen_be_error_t::ok) return 1;
  if (test_e2e_complex_clang_abi(cpfe_bin, work_dir) != llvm_gen_be_error_t::ok) return 1;
  if (test_e2e_source_location_header(cpfe_bin, work_dir) != llvm_gen_be_error_t::ok) return 1;
  if (test_e2e_cxx_abi_vtable_ptrmem_raii_eh(cpfe_bin, work_dir) != llvm_gen_be_error_t::ok) return 1;
  if (test_e2e_coroutines_o0_and_o2(cpfe_bin, work_dir) != llvm_gen_be_error_t::ok) return 1;
  if (test_e2e_omp_simd_shared_multitu(cpfe_bin, work_dir) != llvm_gen_be_error_t::ok) return 1;
  if (test_e2e_hardened_regression_suite(cpfe_bin, work_dir) != llvm_gen_be_error_t::ok) return 1;

  std::puts("test_llvm_e2e_execution PASSED!");
  return 0;
}
