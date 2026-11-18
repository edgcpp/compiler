//type:cp
//options_all:--c++20
template <class, class>
inline constexpr bool is_same_v = false;
template <class _Ty>
inline constexpr bool is_same_v<_Ty, _Ty> = true;

template <typename T1, typename T2>
concept is_same_type = is_same_v<T1, T2>;
template <typename N>
concept is_num = requires{
is_same_type<float, N>;
};

class number {
private:
float _value = 0;
public:
number(float f) {
_value = f;
}
operator float() {
return _value;
}
};

template<is_num T>
number get_num_by(number(*pfunc)(number, number), T n1, is_num auto ... ns) {
number target = n1;
return target;
};
static number get_bigger(number n1, number n2) {
return n1 > n2 ? n1 : n2;
}

int main() {
auto num3 = get_num_by(get_bigger, 7.99f, 5.4f);
return 0;
}
