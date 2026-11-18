//type:cp
//options_all:--g++ --c++17
typedef void (*error_callback)();

template <int T>
class SomeClass {
private:
    static error_callback _cb;

public:
    void Something()
    {
        _cb[0] = nullptr;
    }
};
