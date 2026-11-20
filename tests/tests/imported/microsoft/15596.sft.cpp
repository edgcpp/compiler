//options_all:--microsoft --c++14
struct B
{
template <typename T>
B(T) noexcept;
};

struct D : public B
{
using B::B;
};

static_assert(noexcept(D{1}), "D{1} is NOT noexcept");
