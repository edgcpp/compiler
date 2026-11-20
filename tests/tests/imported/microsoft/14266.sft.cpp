//options_all:--microsoft
template<class T>
struct type1
{
type1 & operator =(const type1 &) = default;
type1(const type1 &) = default;
type1();
};
template<class T> type1<T>::type1() = default; //This line fails
type1<double> t1;
