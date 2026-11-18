//options_all:--g++
//type:fp
template <int I, int J>
struct Matrix {
    template <int K>
    Matrix<I, K> operator*(Matrix<J, K>);
};

template <int I, int J, class T>
Matrix<I, J> operator*(T, Matrix<I, J>);

void f(Matrix<2, 2> matrix1, Matrix<2, 2> matrix2) {
    matrix1 * matrix2;
}
