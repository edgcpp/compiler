//type:fp
//options:--microsoft --ms_c++17:--c++17 --g++ --ms_extensions:--c++17 --clang --ms_extensions
//remark: constexpr defaulted constructors in dllimport classes

// An explicitly constexpr defaulted constructor in a dllimport class with
// virtual functions must be usable during constant evaluation.  GCC accepts
// such construction in a static_assert, but rejects a constexpr object whose
// vtable pointer is not a constant expression.
// This front-end test checks acceptance only.  MSVC can accept a constexpr
// object but fail to link it against a DLL.

// The original reproducer has a virtual destructor.
class __declspec(dllimport) QRunnable {
public:
  constexpr QRunnable() noexcept = default;
  virtual ~QRunnable();
};

struct __declspec(dllimport) ImportedVirtualFunction {
  constexpr ImportedVirtualFunction() noexcept = default;
  virtual void run();
};

#if !defined(__GNUC__) || defined(__clang__)
constexpr ImportedVirtualFunction direct{};
#endif
static_assert((ImportedVirtualFunction{}, true), "");

struct Base {
  virtual void run();
};

// A virtual function inherited from a base also exercises the restriction.
struct __declspec(dllimport) ImportedInheritedVirtualFunction : Base {
  constexpr ImportedInheritedVirtualFunction() noexcept = default;
};

#if !defined(__GNUC__) || defined(__clang__)
constexpr ImportedInheritedVirtualFunction inherited{};
#endif
static_assert((ImportedInheritedVirtualFunction{}, true), "");

// A default member initializer must still pass the usual constexpr checks.
struct __declspec(dllimport) ImportedWithMemberInitializer {
  constexpr ImportedWithMemberInitializer() noexcept = default;
  virtual void run();
  int value = 42;
};

#if !defined(__GNUC__) || defined(__clang__)
constexpr ImportedWithMemberInitializer initialized{};
static_assert(initialized.value == 42, "");
#endif
static_assert(ImportedWithMemberInitializer{}.value == 42, "");

// Control case: without virtual functions, the constructor was already valid.
struct __declspec(dllimport) ImportedWithoutVirtualFunction {
  constexpr ImportedWithoutVirtualFunction() noexcept = default;
  int value = 42;
};

constexpr ImportedWithoutVirtualFunction no_virtual{};
static_assert(no_virtual.value == 42, "");
