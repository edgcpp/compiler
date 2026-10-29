//type:rp
//options:--c++17:--c++17 --g++:--c++17 --g++ -tused:--c++17 --clang
//source_files:a.h use.C inst.C
//script:./check_defs
// The members of A<int> are used here and in use.C, but only inst.C may
// define them; each object used to contain an external definition.
#include "a.h"
int use();
int main() {
  return (A<int>().f() == 1 && A<int>().g(1, 2) == 1 && use() == 2) ? 0 : 1;
}
