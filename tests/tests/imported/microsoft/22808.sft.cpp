//options_all:--c++20
template <class>
concept _Is_from_primary = true;

template <auto>
using conditional_t = int;

template <class _Ty>
using iter_difference_t = conditional_t<_Is_from_primary<_Ty>>;

template <class _Se, class _It>
using sized_sentinel_for = iter_difference_t<_It>;
