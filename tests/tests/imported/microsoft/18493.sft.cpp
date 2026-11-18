//type:fp
//options_all:--microsoft_version 1911
template <class _Type, template <class...> class _Template>
constexpr bool _Is_specialization_v = false;
template <template <class...> class _Template, class... _Types>
constexpr bool _Is_specialization_v<_Template<_Types...>, _Template> = true;
template <class T> struct foo {};
template <class T> struct bar {
  static_assert(_Is_specialization_v<T, foo>, "BOOM");
};
int main() {
    bar<foo<int>> instance;
    (void)instance;
}
