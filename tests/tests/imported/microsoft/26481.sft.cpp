//type:cp
//options_all:--clang_v 150000 --c++14
template <class T>
inline bool foo(T t);

namespace test {
    using ::foo __attribute__((using_if_exists));
}
