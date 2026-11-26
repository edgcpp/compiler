//type:fp
//options_all:--c++20
//options:--clang --clang_version=140000:--clang --clang_version=180000:--strict
//remark:Qualification conversion to an array of unknown bound

using P = int *(*)[3];
using S = const int *const (*)[];

void take(S);

void allow(P p, S s) {
  s = p;
  take(s);
}
