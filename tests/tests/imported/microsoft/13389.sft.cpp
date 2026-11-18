//options_all:--c++14 --microsoft
template<class ...T>
class OUTER
{
public:
template<class ...G>
class INNER;
};

template<class ...T>
template<class ...G>
struct OUTER<T...>::INNER
{
class INNERINNER;
};

template<class ...T>
template<class ...G>
class OUTER<T...>::INNER<G...>::INNERINNER {};

int main() {
OUTER<>::INNER<>::INNERINNER a;
}
