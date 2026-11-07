//type:fp
//options_all:--c++20
//options:--g++ --gnu_version=130300:--strict
using size_type = decltype(sizeof(0));
static_assert(sizeof(size_type) >= sizeof(void*), "pointer width");
size_type wide(void *p) { return reinterpret_cast<size_type>(p); }
