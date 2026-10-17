//type:fp
//options_all:--c++11 --diag_error noreturn_function_does_return

// Do not warn that a noreturn function actually returns
// if it invokes a noreturn constructor or destructor.

struct C {
  [[noreturn]] C() { throw 123; }
};

struct D {
  [[noreturn]] ~D() { throw 456; }
};

[[noreturn]] void fC() { C{}; }  // Previously warned.  Now no diagnostic issued.

[[noreturn]] void fD() { D{}; }  // Previously warned.  Now no diagnostic issued.

struct SC {
  [[noreturn]] SC() { C{}; }  // Previously warned.  Now no diagnostic issued.
};

struct SD {
  [[noreturn]] SD() { D{}; }  // Previously warned.  Now no diagnostic issued.
};
