//options_all:--c++20
//type:fp
namespace std {
    template <class _Derived, class _Base>
    concept derived_from = true;
}

class iterator {
    template <class Category, class T>
    static constexpr bool at_least = std::derived_from<Category, T>;
};
