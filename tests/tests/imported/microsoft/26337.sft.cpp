//type:cp
//options_all:--c++20
using size_t = unsigned int;
using uint8_t = unsigned char;

template <class T, size_t N>
struct array {
    constexpr T* data() { return elems; }
    constexpr const T* data() const { return elems; }

    T elems[N];
};

static constexpr int SERIAL_LEN = 10;
struct serial : public array<uint8_t, SERIAL_LEN> {
    using array<uint8_t, SERIAL_LEN>::array;

    constexpr serial() = default;

    constexpr serial(const serial &other) {
        const uint8_t *it = other.data();
        const uint8_t *end = other.data() + SERIAL_LEN;
        uint8_t *out = this->data();
        for (; it != end; ++out, (void) ++it) { *out = *it; }
    }

    constexpr serial(const array<uint8_t, SERIAL_LEN> &arr) {
        const uint8_t *it = arr.data();
        const uint8_t *end = arr.data() + SERIAL_LEN;
        uint8_t *out = this->data();
        for (; it != end; ++out, (void) ++it) { *out = *it; }
    }
};

template <size_t N>
constexpr serial make_num(const uint8_t (&arr)[N]) {
    serial result;
    const uint8_t *it = arr;
    const uint8_t *end = arr + N;
    uint8_t *out = result.data();
    for (; it != end; ++out, ++it) { *out = *it; }
    return result;
}

void f() {
    constexpr auto s1 = make_num({ 1, 2, 3, 4, 5, 6, 7, 8, 9, 0 });
    constexpr auto s2(s1);
    constexpr auto s3 = s2;
    constexpr auto arr1 = array<uint8_t, SERIAL_LEN>{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 0 };
    constexpr auto s4 = serial(arr1);
    constexpr uint8_t arr2[SERIAL_LEN] = { 1U, };
    constexpr auto s5 = make_num(arr2);
}
