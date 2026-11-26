//type:cp
//options_all:--ms_c++20 --microsoft_v 1940 -w
namespace std {
struct string;

struct _Container_proxy {
  const string *_Mycont;
};

struct iterator {
  _Container_proxy *_Myproxy;
  const char *_Ptr;

  constexpr void operator*() const;
};

union _Bxty {
  char _Buf[16]{};
};

struct string {
  constexpr string() : _Myproxy{new _Container_proxy{this}} {}

  string(const string &) = delete;
  string &operator=(const string &) = delete;

  constexpr ~string() { delete _Myproxy; }

  constexpr iterator cbegin() const { return iterator{_Myproxy, _Bx._Buf}; }

  _Container_proxy *_Myproxy;
  _Bxty _Bx;
};

constexpr void iterator::operator*() const {
  auto _Contptr = _Myproxy->_Mycont->_Bx._Buf;
  (void)(_Ptr < _Contptr + 4);
}
} // namespace std

constexpr bool test() {
  union string_holder {
    constexpr ~string_holder() { s.~string(); }
    std::string s{};
  };
  string_holder sh;
  *sh.s.cbegin();
  return true;
}

static_assert(test());
