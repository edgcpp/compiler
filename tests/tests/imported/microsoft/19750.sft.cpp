//options_all:--microsoft_version 1914
struct Base
{
    virtual void SomeVirtualFunction() = 0;
};

struct Derived : public Base
{
    Derived() {}
    Derived(const Derived &derived) : Base{derived} {}

    void SomeVirtualFunction() override { return; }
};
