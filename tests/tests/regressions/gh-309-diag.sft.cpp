//type:fp
//options:--g++:--g++ --no_exceptions
//options_all:--il_display
//require:DO_IL_LOWERING 1
//filter:awk -v RS='' -v ORS='\n\n' '/\noperation[.]kind: +eok_call\n/' | grep -E '^position[.](seq|column):'
//remark:Diagnostic position of implicit destructor calls at scope exit

// The calls of the destructors of d1, ..., d4 are attributed (in the
// generated code) to the points where the variables are destroyed, but the
// call expressions are positioned at the declarations of the variables
// (which is where diagnostics arising from those calls should be reported).
// Before, they had no position.
struct D {
  ~D();
};
int sink;

void f() {
  D d1;
  sink = 1;
}

void g(bool b) {
  D d2;
  if (b) return;
  {
    D d3;
    sink = 2;
  }
  for (;;) {
    D d4;
    if (sink) break;
  }
  sink = 3;
}
