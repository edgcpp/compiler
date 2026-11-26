//options_all:--c++20
template <typename T, typename U> inline constexpr bool is_same_v = false;
template <typename T> inline constexpr bool is_same_v<T, T> = true;

inline constexpr unsigned int dynamic_extent = static_cast<unsigned int>(-1);

template <class T, unsigned int N = dynamic_extent>
struct span {
    constexpr explicit(N != dynamic_extent) span(T*, unsigned int);
};

int main() {
    int arr[3]{10, 20, 30};
    static_assert(is_same_v<decltype(span{arr, 3}), span<int>>);
}
