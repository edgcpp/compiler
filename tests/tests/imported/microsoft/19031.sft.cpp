//type:fp
//options_all:--ms_c++latest --microsoft_version 1912
template <typename T>
struct variant {
            template <typename U = T>
            variant() noexcept(__is_nothrow_constructible(U)) {}
};

static_assert(__is_nothrow_constructible(variant<int>));
