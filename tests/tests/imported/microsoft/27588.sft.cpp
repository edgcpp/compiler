//type:cp
//options_all:--microsoft
template <class _Ty, size_t _Size>
constexpr size_t size(const _Ty(&)[_Size]) noexcept {
    return _Size;
}

constexpr int arr[100000] = {};
constexpr auto s = size(arr);
