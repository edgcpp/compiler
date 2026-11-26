//type:cp
//options_all:--microsoft_v 1940 --ms_c++20
struct MyInt { int m_value; };


MyInt m(5);

struct MyInt2 {
    MyInt2() : arr(1) {} // IntelliSense complains: E0289 no instance of constructor "MyInt::MyInt" matches the argument list
    MyInt arr;
};
