//type:cp
//options_all:--microsoft_v 1940 --ms_c++latest
// Repros with prod/fe x86chk as of 2023-10-11.
// Affects STL test P0009R18_mdspan_mdspan.

// EDG: Default member initializer is instantiated too eagerly when there's a `noexcept` defaulted default constructor

template <class AccessorPolicy>
struct Mdspan_accessor_base {
    Mdspan_accessor_base() noexcept = default;

    template <class OtherAccessorPolicy>
    constexpr explicit Mdspan_accessor_base(const OtherAccessorPolicy& Acc_) : Acc(Acc_) {}

    static constexpr int rank = 1;

    AccessorPolicy Acc = AccessorPolicy(); // error: no instance of constructor "Accessor::Accessor" matches the argument list
};

struct Accessor {
    constexpr explicit Accessor(int) noexcept {}
};

using Mds = Mdspan_accessor_base<Accessor>;

static_assert(Mds::rank == 1);
