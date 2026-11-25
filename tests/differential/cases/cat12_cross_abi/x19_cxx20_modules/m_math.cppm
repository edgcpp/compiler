export module m_math;
export int triple_add(int a, int b) {
  return 3 * a + b;
}
export struct ModBox {
  int v;
  int scaled(int k) const { return v * k; }
};
