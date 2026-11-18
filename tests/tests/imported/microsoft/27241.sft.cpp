//type:cp
//options_all:-w --microsoft_v 1940 --ms_c++20
struct S {
    char* s;
};

constexpr int intArray1[65536]{};
constexpr int intArray2[65537]{};
constexpr char* charArray1[52428]{};
constexpr char* charArray2[52429]{};
constexpr S strArray1[43690]{};
constexpr S strArray2[43691]{};

int main()
{
    constexpr int a = intArray1[0];
    constexpr int b = intArray2[0]; // intellisense error
    constexpr char* c = charArray1[0];
    constexpr char* d = charArray2[0]; // intellisense error
    constexpr char* e = strArray1[0].s;
    constexpr char* f = strArray2[0].s; // intellisense error
}
