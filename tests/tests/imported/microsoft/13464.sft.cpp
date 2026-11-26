//type:fn
//options_all:--microsoft  --c++14
template <class V, class ...B>
struct C1 {
};

template<class M>
struct K
{
};

struct AA
{
                template <template <class X, class ...C1> class ...A, class X, class ...C1>
                static void ff4(int sizeOFPP, A<X, C1...> ...a)
                {
                }
};

int main()
{
                C1<int, int> c;
                K<int> k;

                AA::ff4(4,c,k); // ambiguous, should be an error
}
