//options_all:--microsoft
struct IUnknown;

template <bool b, typename T = void>
struct EnableIf {};

template <typename T>
struct EnableIf<true, T> {
              typedef T type;
};

template <typename T1, typename T2>
struct IsSame {
              static const bool value = false;
};

template <typename T1>
struct IsSame<T1, T1> {
              static const bool value = true;
};

template <typename T>
struct ComPtr {
    template <typename U>
    void CopyTo(typename EnableIf<IsSame<T, IUnknown>::value && !IsSame<U, T>::value, void>::type * = 0);
};

ComPtr<void> x;
