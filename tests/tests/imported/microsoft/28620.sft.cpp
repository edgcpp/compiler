//type:cp
//options_all:--microsoft_v 1942
//options:--c++26:;fn
struct Matrix {
    Matrix(size_t rows, size_t cols) : row{ rows }, col{ cols } {}

    auto operator[](size_t row, size_t col) -> size_t& {
        return this->row;
    }

    auto operator[](size_t row, size_t col) const -> const size_t& {
        return this->row;
    }

    size_t row;
    size_t col;
};
