//options_all:--microsoft
struct time_point {
    template <class _Duration2>
    constexpr time_point(_Duration2) {}
};

struct __declspec(dllexport) Timestamp : time_point
{
    using time_point::time_point;
};

Timestamp t(0);
