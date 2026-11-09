//type:rp
//module_files:foo.ixx
//options_all:--module_import_diagnostics -d-module_report --c++20 --gnu_version=150100 --modules

import foo;

int main() {
  return bar();
}
