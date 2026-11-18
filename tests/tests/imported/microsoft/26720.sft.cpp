//type:cp
//options_all:--c++20
 template <typename T>
concept c = requires(T::X x) { ++x; };

int main()
{}
