//type:cp
//module_files:foo.ixx
//options_all:--module_import_diagnostics -d-module_report --c++20 --gnu_version=150100 --modules

import foo;

int main() {
  _Float32 a = quarter(8);
  _Float64 b = eighth(16);
  _Float128 c = sixteenth(32);
  return 0;
}
