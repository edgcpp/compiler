//options_all:--gn 110000 --c++17
typedef float __m128 __attribute__((vector_size(16)));

float foo(__m128 rgb) {
    rgb *= 255;
    return rgb[0];
}
