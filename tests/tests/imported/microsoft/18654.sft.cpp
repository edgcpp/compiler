//type:fn
//options_all:--microsoft_version 1911
template<typename... T>
void f(T* ...) {}

template<typename T>
int f(const T&) {}

int main()
{
    int i = 0;
    f(&i);
}
