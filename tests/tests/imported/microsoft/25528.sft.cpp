//options_all:--ms_c++14
void hash(const int&);

template <class To>
struct ConvertibleTo {
    operator To &() &;
    operator const To &() const &;
    operator To &&() &&;
    operator const To &&() const &&;
};

void f() {
    hash(ConvertibleTo<int>{});
}
