//options_all:--microsoft --c++14
template <class _Iter> constexpr _Iter _Unchecked(_Iter _Src) { return (_Src); }
template <class _Iter, class _UIter>
constexpr _Iter &_Rechecked(_Iter &_Dest, const _UIter _Src) {
  return (_Dest = _Src);
}
template <class _FwdIt1, class _FwdIt2>
inline _FwdIt1 find_first_of(const _FwdIt1 _First1, const _FwdIt1 _Last1,  // Note const _Last1 here
                             const _FwdIt2 _First2, const _FwdIt2 _Last2) noexcept;
template <class _FwdIt1, class _FwdIt2>
inline _FwdIt1 find_first_of(const _FwdIt1 _First1, _FwdIt1 _Last1,  // Note non-const _Last1 here
                             const _FwdIt2, const _FwdIt2) noexcept {
  return (_Rechecked(_Last1, _Unchecked(_First1)));
}
int main() {
  int *nil = nullptr;
  find_first_of(nil, nil, nil, nil);
}
