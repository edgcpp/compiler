//options:--microsoft_version 1903:--c++14
class foo
{
    void bar()
    {
        [](decltype(this) param) {};
    }
};
