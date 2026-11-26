//options_all:--c++14 --il
//filter:grep -A5 initializer_range
class B {
    bool b = false;
    void bar(B &b);
};
