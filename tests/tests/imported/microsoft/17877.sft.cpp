//type:fp
//options_all:--microsoft_version 1910
template <class To, class From>
struct is_nothrow_assignable {
  static constexpr bool value = __is_nothrow_assignable(To, From);
};
