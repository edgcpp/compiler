//type:cp
//options:--c++20:--microsoft_v 1950 --ms_c++20
template <typename T>
struct Iterator {
    using Diff = int;
};

struct Sentinel {
    template <typename T>
    friend Iterator<T>::Diff operator-(Sentinel, Iterator<T>) {
        return 0;
    }
};

int main() {
    Sentinel s;
    Iterator<double> i;
    s - i;
}
