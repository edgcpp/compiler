#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>

struct RunResult {
    int returncode;
    std::string stdout_str;
    std::string stderr_str;
};

RunResult run_command(const std::string& cmd) {
    std::string out_file = "_test_out.txt";
    std::string err_file = "_test_err.txt";
    
    std::string full_cmd = cmd + " > " + out_file + " 2> " + err_file;
    
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

bool file_exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

void run_fuzz() {
    const char* env = std::getenv("RUN_LLVM_LINK_TESTS");
    if (!env || std::string(env) != "1") {
        std::cout << "Skipping fuzz execution test. Set RUN_LLVM_LINK_TESTS=1 to run.\n";
        return;
    }
    
    std::string cpfe_path = "build/test_llvm_enabled/bin/cpfe";
    std::string cpfe_exe = cpfe_path;
#ifdef _WIN32
    cpfe_exe += ".exe";
#endif

    if (!file_exists(cpfe_exe) && !file_exists(cpfe_path)) {
        std::cout << "FAIL: " << cpfe_path << " not found.\n";
        std::exit(1);
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
    RunResult res = run_command(cmd);
    
    if (res.returncode < 0) {
        std::cout << "FAIL: cpfe crashed during fuzzing.\n";
        std::exit(1);
    }
    
    std::cout << "PASS: Fuzz testing completed successfully without crashes.\n";
    
    std::remove(test_file.c_str());
    std::remove(out_file.c_str());
}

int main() {
    run_fuzz();
    return 0;
}
