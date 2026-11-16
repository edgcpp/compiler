//type:fp
//options:--microsoft --ms_c++17:--c++17 --g++ --ms_extensions:--c++17 --clang --ms_extensions
//remark: implicit constexpr constructors in dllimport classes

// An implicitly declared default constructor in a dllimport class with
// virtual functions can be used during constant evaluation in Microsoft,
// GNU, and Clang modes.  GCC rejects constexpr objects of these classes
// because their vtable pointers are not constant expressions.
// This front-end test checks acceptance only.  MSVC can accept a constexpr
// object but fail to link it against a DLL.

struct __declspec(dllimport) ImportedWithImplicitDefaultConstructor {
  virtual void run();
};

#if !defined(__GNUC__) || defined(__clang__)
constexpr ImportedWithImplicitDefaultConstructor imported{};
#endif
static_assert((ImportedWithImplicitDefaultConstructor{}, true), "");

struct Base {
  virtual void run();
};

struct __declspec(dllimport) ImportedWithInheritedVirtualFunction : Base {};

#if !defined(__GNUC__) || defined(__clang__)
constexpr ImportedWithInheritedVirtualFunction inherited{};
#endif
static_assert((ImportedWithInheritedVirtualFunction{}, true), "");
