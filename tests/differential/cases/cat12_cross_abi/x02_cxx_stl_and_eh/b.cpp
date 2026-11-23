#include <stdexcept>
#include <string>
#include <vector>
std::string build_greeting(int n) {
  if (n < 0) throw std::runtime_error("bad_n");
  return "hi_" + std::to_string(n);
}
std::vector<int> build_seq(int n) {
  std::vector<int> out;
  for (int i = 1; i <= n; ++i) out.push_back(i * 10);
  return out;
}
