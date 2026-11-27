//type:fp
//options:--microsoft --ms_c++20:--c++20 --clang --ms_extensions:--c++20 --g++ --ms_extensions

//remark: constinit objects of dllimport classes with virtual functions

// This test records two constinit emulation differences.  MSVC compiles
// constexpr objects of these types but rejects both unbraced constinit
// definitions with C2127; EDG Microsoft mode accepts them.  GCC rejects
// both definitions because their vtable pointers are not constant
// expressions; EDG GNU mode accepts them.  Clang and EDG Clang mode accept
// both.

struct __declspec(dllimport) ImportedExplicit {
  constexpr ImportedExplicit() noexcept = default;
  virtual void run();
};

constinit ImportedExplicit explicit_constinit;

struct __declspec(dllimport) ImportedImplicit {
  virtual void run();
};

constinit ImportedImplicit implicit_constinit;
