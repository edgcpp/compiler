//type:fn
//options_all:--microsoft_v 1910
template <class T> class S {
  typedef typename T type;
};

template <class T, typename S<T>::type* = 0>
bool f(T x);

int main()
{
    f(10);
}
