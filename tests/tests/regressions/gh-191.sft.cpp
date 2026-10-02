//options_all:--c++17 --gnu_version=150200
//type:cp
template <bool, typename, typename> using a = int;
template <bool, typename, typename> using b = int;
template <typename...> using i = unsigned long;
template <typename> constexpr bool d = false;
template <typename e, typename f> constexpr bool j = __is_same(e, f);
template <typename e, e...> struct h {};
template <typename e, e ad> using ae = h<e, __integer_pack(ad)...>;
template <long... af> using ag = h<unsigned long, af...>;
template <long ad> using ah = ae<unsigned long, ad>;
template <int ai, typename... aj> struct k {
  using g = __type_pack_element<ai, aj...>;
};
typedef unsigned long al;
template <typename...> class l;
template <typename> long ao;
template <typename... aj> constexpr long ao<l<aj...>> = sizeof...(aj);
template <unsigned long, typename> struct m;
template <unsigned long ai, typename... aj> struct m<ai, l<aj...>> {
  using g = typename k<ai, aj...>::g;
};
template <long ai, typename aq> using ar = typename m<ai, aq>::g;
template <typename... aj> l<aj...> as(l<aj...>);
template <long ai, typename aq, typename at = decltype(aq()),
          typename e = ar<ai, at>>
using au = a<d<aq>, e, e>;
template <typename e, typename... aj> bool ay = (j<e, aj> && ...);
template <typename av, typename aq, unsigned long... az>
constexpr bool ba(ag<az...>) {
  (void)ay<i<av, au<az, aq>>...>;
  return false;
}
template <typename...> class l {};
template <typename av, typename... aw> constexpr av bb(av x, aw...) {
  using bc = decltype(as(aw()...));
  ba<av, bc>(ah<ao<bc>>());
  return x;
}
struct {
  using bd = l<al, long>;
  bd c;
} be;
void e() {
  struct n {};
  bb(n{}, be.c);
}
using bh = b<j<unsigned long, al>, int, long>;
