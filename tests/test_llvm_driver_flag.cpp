#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <algorithm>

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

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}

void test_driver_flag() {
    const char* env = std::getenv("RUN_LLVM_LINK_TESTS");
    if (!env || std::string(env) != "1") {
        std::cout << "Skipping execution test. Set RUN_LLVM_LINK_TESTS=1 to run.\n";
        return;
    }
    
    std::string cpfe_path = "build/test_llvm_enabled/bin/cpfe";
    std::string cpfe_exe = cpfe_path;
#ifdef _WIN32
    cpfe_exe += ".exe";
#endif

    if (!file_exists(cpfe_exe) && !file_exists(cpfe_path)) {
        std::cout << "FAIL: " << cpfe_path << " not found. Please build cpfe first.\n";
        std::exit(1);
    }
    
    std::string test_file = "tests/dummy_test_file.cpp";
    {
        std::ofstream f(test_file);
        f << "int main() { return 0; }\n";
    }
    
    std::string out_file = "tests/dummy_test_file.ll";
    
    std::string cmd = cpfe_path + " --gen_llvm_file_name " + out_file + " " + test_file;
    std::cout << "Running: " << cmd << "\n";
    
    RunResult res = run_command(cmd);
    
    if (res.returncode < 0) {
        std::cout << "FAIL: cpfe crashed.\n";
        std::cout << res.stderr_str << "\n";
        std::exit(1);
    }
    
    std::string err_lower = to_lower(res.stderr_str);
    if (err_lower.find("unrecognized option") != std::string::npos && 
        res.stderr_str.find("gen_llvm_file_name") != std::string::npos) {
        std::cout << "FAIL: cpfe did not recognize the --gen_llvm_file_name flag.\n";
        std::cout << res.stderr_str << "\n";
        std::exit(1);
    }
    
    std::cout << "PASS: cpfe accepted --gen_llvm_file_name flag without crashing.\n";
    
    std::remove(test_file.c_str());
    std::remove(out_file.c_str());
}

int main() {
    test_driver_flag();
    return 0;
}
