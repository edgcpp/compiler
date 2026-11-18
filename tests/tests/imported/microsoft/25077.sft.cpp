//type:fn
//options_all:--c++11
struct A {
    virtual void Foo() {}
};

struct B : public A {
    template <typename T> void Foo() override {}
};
