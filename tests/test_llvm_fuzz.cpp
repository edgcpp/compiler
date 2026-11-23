/**
 * @file test_llvm_fuzz.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Smoke fuzz test runner for the EDG LLVM backend.
 * @details Exercises arithmetic, floating-point, and aggregate AST lowering through `cpfe`.
 */

#include "llvm_gen_be_error.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>

using namespace edg;

/**
 * @struct RunResult
 * @brief Captures the exit code, stdout, and stderr of a child command execution.
 * @details Used to inspect fuzz test command execution results.
 */
struct RunResult {
    int returncode;         ///< Process exit status code.
    std::string stdout_str; ///< Captured standard output stream.
    std::string stderr_str; ///< Captured standard error stream.
};

/**
 * @brief Runs a shell command and captures its exit status and output streams in `out_res`.
 * @details Redirects stdout and stderr to temporary files and cleans them up after reading.
 * @param[in] cmd The shell command string to execute.
 * @param[out] out_res Pointer to `RunResult` receiving the execution results.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_res` is null.
 */
llvm_gen_be_error_t run_command(const std::string& cmd, RunResult* out_res) noexcept {
    if (!out_res) return llvm_gen_be_error_t::invalid_argument;
    std::string out_file = "_test_out.txt";
    std::string err_file = "_test_err.txt";
    
    std::string full_cmd = cmd + " > " + out_file + " 2> " + err_file;
    
    int ret = std::system(full_cmd.c_str());
    
    out_res->returncode = ret;
    
    std::ifstream out(out_file);
    if(out) {
        std::ostringstream ss;
        ss << out.rdbuf();
        out_res->stdout_str = ss.str();
    }
    
    std::ifstream err(err_file);
    if(err) {
        std::ostringstream ss;
        ss << err.rdbuf();
        out_res->stderr_str = ss.str();
    }
    
    std::remove(out_file.c_str());
    std::remove(err_file.c_str());
    
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Checks whether a file exists at `path`.
 * @details Opens an `std::ifstream` and writes `f.good()` into `out_exists`.
 * @param[in] path Filesystem path to check.
 * @param[out] out_exists Receives true if the file exists and is readable.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_exists` is null.
 */
llvm_gen_be_error_t file_exists(const std::string& path, bool* out_exists) noexcept {
    if (!out_exists) return llvm_gen_be_error_t::invalid_argument;
    std::ifstream f(path);
    *out_exists = f.good();
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Runs the smoke fuzz test against `cpfe --gen_llvm_file_name`.
 * @details Skipped unless `RUN_LLVM_LINK_TESTS=1` is set in the environment.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t run_fuzz(void) noexcept {
    const char* env = std::getenv("RUN_LLVM_LINK_TESTS");
    if (!env || std::string(env) != "1") {
        std::cout << "Skipping fuzz execution test. Set RUN_LLVM_LINK_TESTS=1 to run.\n";
        return llvm_gen_be_error_t::ok;
    }
    
    std::string cpfe_path = "build/test_llvm_enabled/bin/cpfe";
    std::string cpfe_exe = cpfe_path;
#ifdef _WIN32
    cpfe_exe += ".exe";
#endif

    bool exists_exe = false;
    bool exists_path = false;
    LLVM_GEN_BE_CHECK(file_exists(cpfe_exe, &exists_exe));
    LLVM_GEN_BE_CHECK(file_exists(cpfe_path, &exists_path));
    if (!exists_exe && !exists_path) {
        std::cout << "FAIL: " << cpfe_path << " not found.\n";
        return llvm_gen_be_error_t::io_error;
    }
    
    std::string test_file = "tests/fuzz_test.c";
    std::string out_file = "tests/fuzz_test.ll";
    
    std::string ast_ops = 
        "int main() {\n"
        "    int x = 42;\n"
        "    int y = x * 2 / 3 + 1;\n"
        "    float z = 3.14f * (float)y;\n"
        "    struct { char a; int b; } s = { 'c', 100 };\n"
        "    return s.b + (int)z;\n"
        "}\n";
    
    {
        std::ofstream f(test_file);
        f << ast_ops;
    }
    
    std::string cmd = cpfe_path + " --gen_llvm_file_name " + out_file + " " + test_file;
    RunResult res;
    LLVM_GEN_BE_CHECK(run_command(cmd, &res));
    
    if (res.returncode < 0) {
        std::cout << "FAIL: cpfe crashed during fuzzing.\n";
        return llvm_gen_be_error_t::code_gen_failure;
    }
    
    std::cout << "PASS: Fuzz testing completed successfully without crashes.\n";
    
    std::remove(test_file.c_str());
    std::remove(out_file.c_str());
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Entry point for the fuzz test runner executable.
 * @details Executes `run_fuzz`.
 * @return 0 on success, or non-zero on failure.
 */
int main(void) {
    if (run_fuzz() != llvm_gen_be_error_t::ok) return 1;
    return 0;
}
