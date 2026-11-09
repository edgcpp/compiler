//type:fp
//options:--g++:--g++ --no_exceptions
//require:BACK_END_IS_C_GEN_BE 1
//filter:awk '/^#(line)? [0-9]+/ { line = $2; next } /_ZN1DD1Ev\(\(/ { print "destructor call on line " line } { line++ }' Test_name.int.c
int sink;

struct D {
  ~D();
};

void f() {
  D d;
  sink = 1;
}  // The destructor call was previously attributed to the line above.

void g(bool b) {
  D d;
  if (b) return;
  sink = 2;
}  // The destructor call was previously attributed to the line above.
