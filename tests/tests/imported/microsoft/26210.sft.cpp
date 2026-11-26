//type:cp
//options_all:--c++20
consteval void foo() {}
consteval bool bar() {
    decltype(auto) f = &foo;
    return true;
}

template <class T>
concept MyConcept = bar();

template <MyConcept T>
struct MyTemplate {};

using My = MyTemplate<int>;
