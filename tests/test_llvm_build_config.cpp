#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#define mkdir _mkdir
#else
#include <sys/stat.h>
#endif

void remove_dir(const std::string& dir) {
    std::string cmd;
#ifdef _WIN32
    cmd = "rmdir /S /Q " + dir;
#else
    cmd = "rm -rf " + dir;
#endif
    std::system(cmd.c_str());
}

void make_dir(const std::string& dir) {
#ifdef _WIN32
    _mkdir(dir.c_str());
#else
    mkdir(dir.c_str(), 0755);
#endif
}

struct RunResult {
    int returncode;
    std::string stdout_str;
    std::string stderr_str;
};

RunResult run_command(const std::string& dir, const std::string& cmd) {
    std::string out_file = dir + "/_test_out.txt";
    std::string err_file = dir + "/_test_err.txt";
    
    std::string full_cmd;
#ifdef _WIN32
    full_cmd = "cd " + dir + " && " + cmd + " > " + out_file + " 2> " + err_file;
#else
    full_cmd = "cd " + dir + " && " + cmd + " > _test_out.txt 2> _test_err.txt";
#endif
    
    int ret = std::system(full_cmd.c_str());
    
    RunResult res;
    res.returncode = ret;
    
    std::ifstream out(out_file);
    if(out) {
        std::ostringstream ss;
        ss << out.rdbuf();
        res.stdout_str = ss.str();
    }
    
    std::ifstream err(err_file);
    if(err) {
        std::ostringstream ss;
        ss << err.rdbuf();
        res.stderr_str = ss.str();
    }
    
    std::remove(out_file.c_str());
    std::remove(err_file.c_str());
    
    return res;
}

void test_missing_llvm_fails_gracefully() {
    std::cout << "Testing missing LLVM configuration...\n";
    std::string build_dir = "build/test_llvm_missing";
    remove_dir(build_dir);
    make_dir("build");
    make_dir(build_dir);
    
    RunResult res = run_command(build_dir, "cmake ../../ -DENABLE_LLVM_BACKEND=TRUE -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE");
    std::string expected_msg = "LLVM IR backend was enabled (ENABLE_LLVM_BACKEND=TRUE), but LLVM was not";
    
    if (res.returncode != 0 && res.stderr_str.find(expected_msg) != std::string::npos) {
        std::cout << "PASS: CMake failed gracefully with the expected error.\n";
    } else {
        std::cout << "FAIL: CMake did not fail gracefully or output the expected error.\n";
        std::exit(1);
    }
}

void test_cpfe_links_without_llvm() {
    const char* env = std::getenv("RUN_LLVM_LINK_TESTS");
    if (!env || std::string(env) != "1") {
        std::cout << "Skipping link tests. Set RUN_LLVM_LINK_TESTS=1 to run.\n";
        return;
    }
    
    std::cout << "Testing build without LLVM...\n";
    std::string build_dir = "build/test_llvm_disabled";
    remove_dir(build_dir);
    make_dir("build");
    make_dir(build_dir);
    
    RunResult res = run_command(build_dir, "cmake ../../ -DENABLE_LLVM_BACKEND=FALSE");
    if (res.returncode != 0) {
        std::cout << "FAIL: CMake config failed for LLVM disabled.\n";
        std::exit(1);
    }
    
    RunResult build_res = run_command(build_dir, "cmake --build . --target cpfe");
    if (build_res.returncode == 0) {
        std::cout << "PASS: cpfe links successfully without LLVM.\n";
    } else {
        std::cout << "FAIL: cpfe failed to link without LLVM.\n";
        std::cout << build_res.stderr_str << "\n";
        std::exit(1);
    }
}

void test_cpfe_links_with_llvm() {
    const char* env = std::getenv("RUN_LLVM_LINK_TESTS");
    if (!env || std::string(env) != "1") {
        return;
    }
    
    std::cout << "Testing build with LLVM...\n";
    std::string build_dir = "build/test_llvm_enabled";
    remove_dir(build_dir);
    make_dir("build");
    make_dir(build_dir);
    
    RunResult res = run_command(build_dir, "cmake ../../ -DENABLE_LLVM_BACKEND=TRUE");
    if (res.returncode != 0) {
        std::cout << "FAIL: CMake config failed for LLVM enabled.\n";
        std::exit(1);
    }
    
    RunResult build_res = run_command(build_dir, "cmake --build . --target cpfe");
    if (build_res.returncode == 0) {
        std::cout << "PASS: cpfe links successfully with LLVM.\n";
    } else {
        std::cout << "FAIL: cpfe failed to link with LLVM.\n";
        std::cout << build_res.stderr_str << "\n";
        std::exit(1);
    }
}

int main() {
    test_missing_llvm_fails_gracefully();
    test_cpfe_links_without_llvm();
    test_cpfe_links_with_llvm();
    std::cout << "All Phase 1 tests passed!\n";
    return 0;
}
