//type:fn
//options:--c++20 --clang --clang_version=130000:--c++17 --clang --clang_version=140000
//remark:Qualification conversion to an unknown bound before its support

using P = int *(*)[3];
using S = const int *const (*)[];

void take(S);

void reject(P p, S s) {
  s = p;
  take(s);
}
