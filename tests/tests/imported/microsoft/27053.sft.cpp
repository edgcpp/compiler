//type:cp
//options_all:--ms_c++20 --microsoft_v 1940 -w
namespace std {
struct vector;
struct iterator;

struct _Container_proxy { // store head of iterator chain and back pointer
  const vector *_Mycont = nullptr;
  iterator *_Myfirstiter = nullptr;
};

struct iterator {
  constexpr iterator(int *_Parg, const vector *_Pvector) : _Ptr(_Parg) {
    _Adopt(_Pvector);
  }

  constexpr iterator(const iterator &_Right) { *this = _Right; }

  constexpr iterator &operator=(const iterator &_Right) {
    if (_Myproxy != _Right._Myproxy) {
      if (_Right._Myproxy) {
        _Adopt(_Right._Myproxy->_Mycont);
      } else { // becoming invalid, disown current parent
        _Orphan_me();
      }
    }
    _Ptr = _Right._Ptr;
    return *this;
  }

  constexpr ~iterator() { _Orphan_me(); }

  constexpr void _Adopt(const vector *_Parent);

  constexpr void _Orphan_me() {
    if (!_Myproxy) { // already orphaned
      return;
    }

    // adopted, remove self from list
    for (iterator **_Pnext = &_Myproxy->_Myfirstiter;;
         _Pnext = &(*_Pnext)->_Mynextiter) {
      if (*_Pnext == this) {
        *_Pnext = _Mynextiter;
        break;
      }
    }

    _Myproxy = nullptr;
  }

  _Container_proxy *_Myproxy = nullptr;
  iterator *_Mynextiter = nullptr;

  int *_Ptr = nullptr;
};

struct vector {
  constexpr vector() : _Myproxy{new _Container_proxy{this}} {}

  vector(const vector &) = delete;
  vector &operator=(const vector &) = delete;

  constexpr ~vector() {
    // Unlink all iterators
    auto _Pnext = _Myproxy->_Myfirstiter;
    _Myproxy->_Myfirstiter = nullptr;
    for (; _Pnext; _Pnext = _Pnext->_Mynextiter) {
      _Pnext->_Myproxy = nullptr;
    }

    delete _Myproxy;
  }

  constexpr iterator cbegin() const { return iterator(_Myfirst, this); }

  _Container_proxy *_Myproxy = nullptr;

  int *_Myfirst = nullptr;
  int *_Mylast = nullptr;
  int *_Myend = nullptr;
};

constexpr void iterator::_Adopt(const vector *_Parent) {
  if (!_Parent) {
    _Orphan_me();
    return;
  }

  _Container_proxy *_Parent_proxy = _Parent->_Myproxy;
  if (_Myproxy != _Parent_proxy) { // change parentage
    if (_Myproxy) {                // adopted, remove self from list
      _Orphan_me();
    }
    _Mynextiter = _Parent_proxy->_Myfirstiter;
    _Parent_proxy->_Myfirstiter = this;
    _Myproxy = _Parent_proxy;
  }
}
} // namespace std

struct fold_left_result {
  std::iterator in;
};

constexpr auto fold_left_with_iter_impl(std::iterator &&first) {
  return fold_left_result{first};
}

constexpr auto fold_left_with_iter(const std::vector &vec) {
  return fold_left_with_iter_impl(vec.cbegin());
}

constexpr bool non_dependent() {
  std::vector vec;
  fold_left_with_iter(vec);
  return true;
}

static_assert(non_dependent());
