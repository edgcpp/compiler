//type:fn
//options_all:--c++17 --parse --microsoft
template <class _Mutex>
struct lock_guard {
    lock_guard(_Mutex &_Mtx) {}
};

template <typename T>
void func() {
    lock_guard g;
}
