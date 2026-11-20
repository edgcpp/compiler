//type:cp
//options_all:--c++23
//options::-A:--microsoft_v 1950:--gn 999999:--clang_v 999999
template<typename>
class function;

template<typename R, typename... Args>
class function<R(Args...)> {
  struct callable_base {
    virtual R invoke(Args...) = 0;
    virtual callable_base* clone() const = 0;
    virtual ~callable_base() {}
  };

  template<typename F>
  struct callable_impl : callable_base {
    F f;
    callable_impl(F&& func) : f(static_cast<F&&>(func)) {}
    callable_impl(const callable_impl& other) : f(other.f) {}
    R invoke(Args... args) override { return f(static_cast<Args&&>(args)...); }
    callable_base* clone() const override { return new callable_impl<F>(*this); }
  };

  callable_base* ptr = nullptr;

public:
  function() = default;

  template<typename F>
  function(F&& f) : ptr(new callable_impl<F>(static_cast<F&&>(f))) {}

  function(const function& other) : ptr(other.ptr ? other.ptr->clone() : nullptr) {}

  function(function&& other) : ptr(other.ptr) { other.ptr = nullptr; }

  function& operator=(const function& other) {
    if (this != &other) {
      delete ptr;
      ptr = other.ptr ? other.ptr->clone() : nullptr;
    }
    return *this;
  }

  function& operator=(function&& other) {
    if (this != &other) {
      delete ptr;
      ptr = other.ptr;
      other.ptr = nullptr;
    }
    return *this;
  }

  ~function() { delete ptr; }

  R operator()(Args... args) { return ptr->invoke(static_cast<Args&&>(args)...); }

  explicit operator bool() const { return ptr != nullptr; }
};

// example #1: works
function<void()> a = [] [[deprecated]] () {};

// example #2: works
auto b = function<void()>{[] () {}};

// example #3: works
auto c = function<void()>{[] ()[[deprecated]] {}};

// example #4: doesn't work
auto d = function<void()>{[] [[deprecated]] (){}};

// example #5 : doesn't work
auto e = function<void()>{[] [[deprecated]] {}};
