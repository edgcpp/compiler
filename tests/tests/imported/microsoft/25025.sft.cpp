//options_all:--c++17
struct Dog {};
Dog operator ""_dog(const char *);

template <class>
void make_dog() { 0.5_dog; }
