//type: rp
//options: --c++26:--c++26 -tused
//remark:[GH #216] Dependent tuple get calls do not reuse unrelated calls.

namespace N {
  struct A {};
  constexpr void helper(A) {}
  template<typename T> struct Holder { T value; };
  template<unsigned long I, typename T>
  constexpr T& get(Holder<T>& h) { return h.value; }
}
namespace std {
  template<typename> struct tuple_size;
  template<unsigned long, typename> struct tuple_element;
  template<typename T> struct tuple_size<N::Holder<T>> {
    static constexpr unsigned long value = 1;
  };
  template<unsigned long I, typename T> struct tuple_element<I, N::Holder<T>> {
    using type = T;
  };
}
template<typename T>
constexpr int f() {
  N::A a;
  helper(a);
  N::Holder<T> h{3};
  int n = 0;
  /* Keep the padding: it makes the synthesized get call and helper(a) share
     a token sequence number, testing that replay does not reuse helper(a). */
































































































































































































































































































  template for (auto x : h) { n += x; }
  return n;
}
static_assert(f<int>() == 3);
int main() { return f<int>() != 3; }
