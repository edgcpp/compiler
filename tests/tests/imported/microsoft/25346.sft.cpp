//options_all:--c++20
//options::--microsoft_v 1930:--microsoft_v 1930 --parse
struct Record {
    template <class T>
    Record &operator<<(const T &);
};

template <class D, class S>
concept StreamInsertable = requires(S &os, const D &data) { os << data; };

template <class T>
void operator<<(Record &rec, const T &o) requires (!StreamInsertable<T, Record>) {
    rec << 0;
}
