//type:rp
//module_files:foo.ixx
//options_all:--module_import_diagnostics -d-module_report --c++20 --gnu_version=150100 --modules

import foo;

int main() {
  if (quad(2) != 8) return 1;
  if (f32x_half(6) != 3) return 2;
  if (f64x_half(10) != 5) return 3;
  if (kind(0.0) != 1) return 4;
  if (kind((_Float32x)0) != 2) return 5;
  if (kind(0.0L) != 3) return 6;
  if (kind((_Float64x)0) != 4) return 7;
  if (kind((__float128)0) != 5) return 8;
  return 0;
}
