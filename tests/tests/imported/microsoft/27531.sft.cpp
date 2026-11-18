//type:cp
//options_all:--microsoft_v 1940 --ms_c++latest
class C {
public:
                constexpr int n() { return 1; };
};

template<typename T>
concept C1 = requires(T v)
{
                requires v.n() == 1;
};

template<typename T> requires C1<T>
void foo(T t){}

int main()
{
                foo(C());
}
